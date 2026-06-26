#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <M5StackChan.h>
#include "ControlProtocol.h"

const uint8_t crawlerMac[] = M5CRAWLER_CRAWLER_MAC;
bool restrictCrawlerMac = false;

BodyCommand currentCommand;
unsigned long lastRecvTime = 0;
uint16_t lastSequence = 0;
bool commandReceived = false;
char selfMacText[18] = "00:00:00:00:00:00";

int16_t lastAppliedYaw = 32767;
int16_t lastAppliedPitch = 32767;

int16_t stackchanYawToServoUnit(int16_t yawDeg) {
  return static_cast<int16_t>(constrain(static_cast<int>(yawDeg) * 10, -900, 900));
}

int16_t stackchanPitchToServoUnit(int16_t pitchDeg) {
  return static_cast<int16_t>(constrain(static_cast<int>(pitchDeg) * 10, -900, 900));
}

void applyBodyCommand(const BodyCommand& cmd) {
  int16_t yaw = cmd.moving ? cmd.yaw : 0;
  int16_t pitch = cmd.pitch;

  if (yaw == lastAppliedYaw && pitch == lastAppliedPitch) {
    return;
  }

  lastAppliedYaw = yaw;
  lastAppliedPitch = pitch;

  int16_t servoX = stackchanYawToServoUnit(yaw);
  int16_t servoY = stackchanPitchToServoUnit(pitch);
  M5StackChan.Motion.move(servoX, servoY, 500);
}

void drawStatus() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);

  int cx = M5.Display.width() / 2;
  int cy = M5.Display.height() / 2;
  uint16_t color = commandReceived ? GREEN : RED;
  if (millis() - lastRecvTime > M5CRAWLER_BODY_TIMEOUT_MS) {
    color = 0xFD20;  // orange
  }

  M5.Display.setTextColor(color, BLACK);
  M5.Display.drawString("Stack-chan", cx, cy - 40);

  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.drawString(commandReceived ? "BODY LINK" : "WAIT BODY", cx, cy - 20);

  char buf[32];
  snprintf(buf, sizeof(buf), "Yaw:%d Pitch:%d", currentCommand.yaw, currentCommand.pitch);
  M5.Display.drawString(buf, cx, cy);
  snprintf(buf, sizeof(buf), "Seq:%u", lastSequence);
  M5.Display.drawString(buf, cx, cy + 20);
  M5.Display.drawString(selfMacText, cx, cy + 42);

  M5.Display.setTextDatum(top_left);
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnBodyRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming, int len) {
  const uint8_t* srcMac = recv_info ? recv_info->src_addr : nullptr;
#else
void OnBodyRecv(const uint8_t *mac, const uint8_t *incoming, int len) {
  const uint8_t* srcMac = mac;
#endif
  if (restrictCrawlerMac && (srcMac == nullptr || !macEquals(srcMac, crawlerMac))) {
    return;
  }

  if (len != sizeof(BodyCommand)) {
    return;
  }

  BodyCommand cmd;
  memcpy(&cmd, incoming, sizeof(cmd));
  if (!validateBodyCommand(cmd)) {
    return;
  }

  currentCommand = cmd;
  lastSequence = cmd.sequence;
  lastRecvTime = millis();
  commandReceived = true;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.fillScreen(BLACK);

  initBodyCommand(currentCommand);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.drawString("Stack-chan", M5.Display.width() / 2, M5.Display.height() / 2 - 20);
  M5.Display.drawString("Starting...", M5.Display.width() / 2, M5.Display.height() / 2);
  M5.Display.setTextDatum(top_left);

  M5StackChan.begin();
  M5StackChan.Motion.goHome(500);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(M5CRAWLER_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  restrictCrawlerMac = !isBroadcastMac(crawlerMac);
  uint8_t selfMac[6];
  WiFi.macAddress(selfMac);
  formatMac(selfMac, selfMacText, sizeof(selfMacText));

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnBodyRecv);
  }
}

void loop() {
  M5.update();

  if (commandReceived && millis() - lastRecvTime <= M5CRAWLER_BODY_TIMEOUT_MS) {
    applyBodyCommand(currentCommand);
  } else if (millis() - lastRecvTime > M5CRAWLER_BODY_TIMEOUT_MS) {
    BodyCommand idle;
    initBodyCommand(idle);
    applyBodyCommand(idle);
  }

  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 500) {
    lastDisplayUpdate = millis();
    drawStatus();
  }

  delay(1);
}
