#include <M5Unified.h>
#include "M5HatMiniJoyC.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "ControlProtocol.h"

// MiniJoyC I2C pins
#define MiniJoyC_SDA 0
#define MiniJoyC_SCL 26

uint8_t peerAddress[] = M5CRAWLER_CRAWLER_MAC;
bool useBroadcast = true;

ControlData data;
M5HatMiniJoyC joyc;

int maxSpeed = M5CRAWLER_SPEED_INITIAL;
unsigned long lastSendTime = 0;
unsigned long lastDisplayTime = 0;
bool joyDetected = false;
esp_err_t lastSendResult = ESP_OK;

// Startup neutral offsets.
int8_t offsetX = 0;
int8_t offsetY = 0;
uint16_t sequence = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Currently the immediate esp_now_send() result is used for display.
}

uint16_t speedGaugeColor(int index) {
  if (index < 3) return GREEN;
  if (index < 5) return 0xFD20;  // orange
  return RED;
}

void drawSignalIcon(int x, int y, bool ok) {
  const uint16_t color = ok ? GREEN : RED;
  const uint16_t dim = 0x3186;
  for (int i = 0; i < 3; i++) {
    int barH = 5 + i * 5;
    int barX = x + i * 7;
    M5.Display.fillRoundRect(barX, y + (15 - barH), 5, barH, 2, ok ? color : dim);
  }
  if (!ok) {
    M5.Display.drawLine(x, y, x + 20, y + 16, RED);
    M5.Display.drawLine(x + 20, y, x, y + 16, RED);
  }
}

void drawModeIcon(int x, int y) {
  if (useBroadcast) {
    M5.Display.drawCircle(x, y, 3, 0xFD20);
    M5.Display.drawCircle(x, y, 8, 0xFD20);
    M5.Display.drawCircle(x, y, 13, 0xFD20);
  } else {
    M5.Display.fillCircle(x - 8, y, 4, CYAN);
    M5.Display.fillCircle(x + 8, y, 4, CYAN);
    M5.Display.drawLine(x - 4, y, x + 4, y, CYAN);
  }
}

void drawJoystickPad(int x, int y) {
  const int padR = min((M5.Display.width() / 2) - 10, 54);
  const int dotR = 6;
  const uint16_t grid = 0x39E7;
  const uint16_t frame = joyDetected ? WHITE : RED;

  M5.Display.drawCircle(x, y, padR, frame);
  M5.Display.drawCircle(x, y, padR / 2, grid);
  M5.Display.drawFastHLine(x - padR, y, padR * 2 + 1, grid);
  M5.Display.drawFastVLine(x, y - padR, padR * 2 + 1, grid);

  // Direction hints.
  M5.Display.fillTriangle(x, y - padR + 6, x - 5, y - padR + 17, x + 5, y - padR + 17, GREEN);
  M5.Display.fillTriangle(x - padR + 6, y, x - padR + 17, y - 5, x - padR + 17, y + 5, CYAN);
  M5.Display.fillTriangle(x + padR - 6, y, x + padR - 17, y - 5, x + padR - 17, y + 5, CYAN);

  if (!joyDetected) {
    M5.Display.drawLine(x - 18, y - 18, x + 18, y + 18, RED);
    M5.Display.drawLine(x + 18, y - 18, x - 18, y + 18, RED);
    return;
  }

  // Display-only orientation:
  // Match the current screen rotation so that pushing the joystick forward
  // moves the dot toward the top of the screen.
  const int limit = padR - dotR - 2;
  const int px = x + (data.x * limit) / 100;
  const int py = y - (data.y * limit) / 100;

  M5.Display.drawLine(x, y, px, py, CYAN);
  M5.Display.fillCircle(px, py, dotR + 2, BLACK);
  M5.Display.drawCircle(px, py, dotR + 2, CYAN);
  M5.Display.fillCircle(px, py, dotR, CYAN);
}

void drawSpeedGauge(int x, int y) {
  const int level = constrain((maxSpeed - M5CRAWLER_SPEED_MIN) / M5CRAWLER_SPEED_STEP + 1, 1, 6);
  const int barW = 13;
  const int gap = 5;
  const int baseY = y + 50;

  for (int i = 0; i < 6; i++) {
    int h = 10 + i * 6;
    int bx = x + i * (barW + gap);
    int by = baseY - h;
    uint16_t color = (i < level) ? speedGaugeColor(i) : 0x3186;
    M5.Display.fillRoundRect(bx, by, barW, h, 3, color);
  }

  // A button hint: small round plus button.
  int btnX = M5.Display.width() - 18;
  int btnY = baseY - 8;
  M5.Display.drawCircle(btnX, btnY, 9, WHITE);
  M5.Display.drawFastHLine(btnX - 4, btnY, 9, WHITE);
  M5.Display.drawFastVLine(btnX, btnY - 4, 9, WHITE);
}

void drawControllerStatus() {
  // Graphical portrait UI for the M5StickC Plus2 135x240 display.
  const int w = M5.Display.width();
  M5.Display.fillScreen(BLACK);

  drawSignalIcon(8, 7, lastSendResult == ESP_OK);
  drawModeIcon(w - 20, 16);
  M5.Display.drawFastHLine(4, 31, w - 8, 0x39E7);

  drawJoystickPad(w / 2, 91);
  drawSpeedGauge(10, 166);
}

void showMacAddressOnDisplay() {
  uint8_t selfMac[6];
  char selfMacText[18];
  char peerMacText[18];

  WiFi.macAddress(selfMac);
  formatMac(selfMac, selfMacText, sizeof(selfMacText));
  formatMac(peerAddress, peerMacText, sizeof(peerMacText));

  M5.Display.setTextColor(WHITE);
  M5.Display.println("STA MAC:");
  M5.Display.println(selfMacText);
  M5.Display.println(useBroadcast ? "Peer: Broadcast" : "Peer MAC:");
  if (!useBroadcast) {
    M5.Display.println(peerMacText);
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  initControlData(data);

  M5.Display.setRotation(0);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE);
  M5.Display.println("Initializing MiniJoyC...");

  if (joyc.begin(&Wire, MiniJoyC_ADDR, MiniJoyC_SDA, MiniJoyC_SCL, 100000UL)) {
    joyDetected = true;
    M5.Display.setTextColor(GREEN);
    M5.Display.println("MiniJoyC Hat: FOUND");

    M5.Display.setTextColor(YELLOW);
    M5.Display.println("Calibrating...");
    M5.Display.println("DO NOT TOUCH JOYSTICK");

    long sumX = 0;
    long sumY = 0;
    const int samples = 10;
    for (int i = 0; i < samples; i++) {
      sumX += static_cast<int8_t>(joyc.getPOSValue(POS_X, _8bit));
      sumY += static_cast<int8_t>(joyc.getPOSValue(POS_Y, _8bit));
      delay(50);
    }
    offsetX = static_cast<int8_t>(sumX / samples);
    offsetY = static_cast<int8_t>(sumY / samples);
    M5.Display.printf("Offset: X=%d, Y=%d\n", offsetX, offsetY);
  } else {
    joyDetected = false;
    data.x = 0;
    data.y = 0;
    M5.Display.setTextColor(RED);
    M5.Display.println("MiniJoyC Hat: NOT FOUND");
  }
  M5.Display.setTextColor(WHITE);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(M5CRAWLER_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  useBroadcast = isBroadcastMac(peerAddress);
  showMacAddressOnDisplay();

  if (esp_now_init() != ESP_OK) {
    M5.Display.println("ESP-NOW Init Failed");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerAddress, 6);
  peerInfo.channel = M5CRAWLER_ESPNOW_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    M5.Display.println("Failed to add peer");
  }

  M5.Display.println("Setup Done.");
  delay(1500);
}

void loop() {
  M5.update();

  if (joyDetected) {
    int8_t pos_x = static_cast<int8_t>(joyc.getPOSValue(POS_X, _8bit));
    int8_t pos_y = static_cast<int8_t>(joyc.getPOSValue(POS_Y, _8bit));

    int corrected_x = pos_x - offsetX;
    int corrected_y = pos_y - offsetY;

    corrected_x = constrain(corrected_x, -128, 127);
    corrected_y = constrain(corrected_y, -128, 127);

    data.x = map(corrected_x, -128, 127, M5CRAWLER_JOY_MIN, M5CRAWLER_JOY_MAX);
    data.y = map(corrected_y, -128, 127, M5CRAWLER_JOY_MAX, M5CRAWLER_JOY_MIN);

    if (abs(data.x) < M5CRAWLER_DEADZONE) data.x = 0;
    if (abs(data.y) < M5CRAWLER_DEADZONE) data.y = 0;

    uint8_t r = abs(data.y) * 2;
    uint8_t b = abs(data.x) * 2;
    joyc.setLEDColor((r << 16) | b);
  } else {
    data.x = 0;
    data.y = 0;
  }

  if (M5.BtnA.wasPressed()) {
    maxSpeed += M5CRAWLER_SPEED_STEP;
    if (maxSpeed > M5CRAWLER_SPEED_MAX) maxSpeed = M5CRAWLER_SPEED_MIN;
  }
  data.maxSpd = maxSpeed;

  if (millis() - lastSendTime > M5CRAWLER_SEND_INTERVAL_MS) {
    lastSendTime = millis();
    data.sequence = sequence++;
    updateControlChecksum(data);
    lastSendResult = esp_now_send(peerAddress, reinterpret_cast<uint8_t*>(&data), sizeof(data));
  }

  if (millis() - lastDisplayTime > M5CRAWLER_DISPLAY_INTERVAL_MS) {
    lastDisplayTime = millis();
    drawControllerStatus();
  }
}
