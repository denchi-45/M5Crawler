#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Avatar.h>
#include <math.h>
#include "SCServo.h"
#include "ControlProtocol.h"

using namespace m5avatar;
Avatar avatar;

const uint8_t allowedControllerMac[] = M5CRAWLER_CONTROLLER_MAC;
uint8_t stackchanAddress[] = M5CRAWLER_STACKCHAN_MAC;
bool restrictControllerMac = false;
bool useStackchanBroadcast = true;

ControlData incomingData;
BodyCommand bodyCommand;

SMS_STS sts;
const int SERVO_LEFT_ID = 1;
const int SERVO_RIGHT_ID = 2;
const int SERVO_TOP_LEFT_ID = 3;
const int SERVO_TOP_RIGHT_ID = 4;

// Servo direction coefficients. Adjust these if the physical mounting changes.
const int DIR_LEFT = 1;
const int DIR_RIGHT = -1;
const int DIR_TOP_LEFT = -1;
const int DIR_TOP_RIGHT = 1;

const int RX_PIN = 2;  // M5 RX <- servo bus TX
const int TX_PIN = 1;  // M5 TX -> servo bus RX

int joyX = 0;
int joyY = 0;
int speedL = 0;
int speedR = 0;
int maxSpeed = M5CRAWLER_SPEED_INITIAL;

unsigned long lastDisplayUpdate = 0;
unsigned long lastRecvTime = 0;
uint16_t lastSequence = 0;
uint16_t bodySequence = 0;
unsigned long lastBodySendTime = 0;

bool servoInitOk = true;
uint8_t servoInitFailureMask = 0;
uint32_t invalidPacketCount = 0;
uint32_t rejectedMacCount = 0;
esp_err_t lastBodySendResult = ESP_OK;

bool initServo(uint8_t id, uint8_t bit) {
  bool ok = true;
  ok = (sts.unLockEprom(id) == 1) && ok;
  ok = (sts.WheelMode(id) == 1) && ok;
  ok = (sts.EnableTorque(id, 1) == 1) && ok;
  if (!ok) {
    servoInitFailureMask |= bit;
  }
  delay(50);
  return ok;
}

void writeServoSpeed(uint8_t id, int speed) {
  sts.WriteSpe(id, static_cast<s16>(speed), 0);
}

int16_t calculateStackchanYaw(int x, int y) {
  // Continuous "look toward the motion/turn intention" mapping.
  //
  // - Straight forward/backward: 0 deg (front)
  // - Gentle turn while moving: small yaw
  // - Strong turn while moving: diagonal yaw
  // - In-place rotation: +/-90 deg (side)
  //
  // Use abs(y) so reverse straight motion does not command an unreachable
  // backward-looking pose. If backward-looking behavior is needed later,
  // this is the place to extend the mapping.
  const int dead = M5CRAWLER_DEADZONE * 2;
  if (abs(x) <= dead && abs(y) <= dead) {
    return 0;
  }

  float yaw = atan2f(static_cast<float>(x), static_cast<float>(abs(y))) * 180.0f / PI;
  return static_cast<int16_t>(constrain(static_cast<int>(roundf(yaw)),
                                       -M5CRAWLER_BODY_YAW_MAX,
                                       M5CRAWLER_BODY_YAW_MAX));
}

void updateAndSendBodyCommand() {
  if (millis() - lastBodySendTime <= M5CRAWLER_BODY_SEND_INTERVAL_MS) {
    return;
  }
  lastBodySendTime = millis();

  const int dead = M5CRAWLER_DEADZONE * 2;
  const bool moving = (abs(joyX) > dead) || (abs(joyY) > dead);

  bodyCommand.sequence = bodySequence++;
  bodyCommand.pitch = M5CRAWLER_BODY_PITCH_DEFAULT;
  bodyCommand.moving = moving ? 1 : 0;

  if (moving) {
    bodyCommand.yaw = calculateStackchanYaw(joyX, joyY);
  } else {
    bodyCommand.yaw = 0;
  }

  updateBodyChecksum(bodyCommand);
  lastBodySendResult = esp_now_send(stackchanAddress,
                                    reinterpret_cast<uint8_t*>(&bodyCommand),
                                    sizeof(bodyCommand));
}

// Round display: draw the whole startup screen as a vertically-centered block
// so that no line is clipped by the circular bezel.
void showStartupScreen(bool espNowOk) {
  uint8_t selfMac[6];
  char selfMacText[18];
  char controllerMacText[18];
  char stackchanMacText[18];

  WiFi.macAddress(selfMac);
  formatMac(selfMac, selfMacText, sizeof(selfMacText));
  formatMac(allowedControllerMac, controllerMacText, sizeof(controllerMacText));
  formatMac(stackchanAddress, stackchanMacText, sizeof(stackchanMacText));

  char espNowText[24];
  char servoText[24];
  char failText[24];
  snprintf(espNowText, sizeof(espNowText), "ESP-NOW: %s", espNowOk ? "OK" : "FAIL");
  snprintf(servoText, sizeof(servoText), "Servo: %s", servoInitOk ? "OK" : "ERROR");
  snprintf(failText, sizeof(failText), "Fail mask: 0x%02X", servoInitFailureMask);

  struct Line {
    const char* text;
    uint16_t color;
  };
  Line lines[10];
  int count = 0;
  lines[count++] = {"CRAWLER", WHITE};
  lines[count++] = {"STA MAC:", WHITE};
  lines[count++] = {selfMacText, CYAN};
  lines[count++] = {restrictControllerMac ? "Controller MAC:" : "Controller: ANY", WHITE};
  if (restrictControllerMac) {
    lines[count++] = {controllerMacText, CYAN};
  }
  lines[count++] = {useStackchanBroadcast ? "StackChan: Broadcast" : "StackChan MAC:", WHITE};
  if (!useStackchanBroadcast) {
    lines[count++] = {stackchanMacText, CYAN};
  }
  lines[count++] = {espNowText, static_cast<uint16_t>(espNowOk ? GREEN : RED)};
  lines[count++] = {servoText, static_cast<uint16_t>(servoInitOk ? GREEN : RED)};
  if (!servoInitOk) {
    lines[count++] = {failText, RED};
  }

  const int lineHeight = 16;
  const int cx = M5.Display.width() / 2;
  const int cy = M5.Display.height() / 2;

  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(middle_center);
  int startY = cy - ((count - 1) * lineHeight) / 2;
  for (int i = 0; i < count; i++) {
    M5.Display.setTextColor(lines[i].color, BLACK);
    M5.Display.drawString(lines[i].text, cx, startY + i * lineHeight);
  }
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(WHITE, BLACK);
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming, int len) {
  const uint8_t* srcMac = recv_info ? recv_info->src_addr : nullptr;
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incoming, int len) {
  const uint8_t* srcMac = mac;
#endif
  if (restrictControllerMac && (srcMac == nullptr || !macEquals(srcMac, allowedControllerMac))) {
    rejectedMacCount++;
    return;
  }

  if (len != sizeof(ControlData)) {
    invalidPacketCount++;
    return;
  }

  ControlData received;
  memcpy(&received, incoming, sizeof(received));
  if (!validateControlData(received)) {
    invalidPacketCount++;
    return;
  }

  incomingData = received;
  joyX = constrain(static_cast<int>(received.x), M5CRAWLER_JOY_MIN, M5CRAWLER_JOY_MAX);
  joyY = constrain(static_cast<int>(received.y), M5CRAWLER_JOY_MIN, M5CRAWLER_JOY_MAX);
  maxSpeed = constrain(static_cast<int>(received.maxSpd), M5CRAWLER_SPEED_MIN, M5CRAWLER_SPEED_MAX);
  lastSequence = received.sequence;
  lastRecvTime = millis();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  initControlData(incomingData);
  initBodyCommand(bodyCommand);

  M5.Display.setRotation(2);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(M5CRAWLER_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  restrictControllerMac = !isBroadcastMac(allowedControllerMac);
  useStackchanBroadcast = isBroadcastMac(stackchanAddress);

  Serial1.begin(1000000, SERIAL_8N1, RX_PIN, TX_PIN);
  sts.pSerial = &Serial1;
  sts.IOTimeOut = 10;

  servoInitOk = true;
  servoInitOk = initServo(SERVO_LEFT_ID, 0x01) && servoInitOk;
  servoInitOk = initServo(SERVO_RIGHT_ID, 0x02) && servoInitOk;
  servoInitOk = initServo(SERVO_TOP_LEFT_ID, 0x04) && servoInitOk;
  servoInitOk = initServo(SERVO_TOP_RIGHT_ID, 0x08) && servoInitOk;

  Serial.printf("Servo init: %s, fail mask=0x%02X\n",
                servoInitOk ? "OK" : "ERROR",
                servoInitFailureMask);

  bool espNowOk = false;
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  } else {
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_peer_info_t stackPeer = {};
    memcpy(stackPeer.peer_addr, stackchanAddress, 6);
    stackPeer.channel = M5CRAWLER_ESPNOW_CHANNEL;
    stackPeer.encrypt = false;
    esp_err_t peerResult = esp_now_add_peer(&stackPeer);
    if (peerResult != ESP_OK && peerResult != ESP_ERR_ESPNOW_EXIST) {
      Serial.println("Failed to add Stack-chan peer");
    }
    espNowOk = true;
  }

  showStartupScreen(espNowOk);
  delay(4000);

  avatar.init();
  avatar.setScale(0.75);
  avatar.setPosition(0, -40);
}

void loop() {
  M5.update();

  if (millis() - lastRecvTime > M5CRAWLER_RECV_TIMEOUT_MS) {
    joyX = 0;
    joyY = 0;
  }

  int targetL = joyY - joyX;
  int targetR = joyY + joyX;

  targetL = constrain(targetL, -200, 200);
  targetR = constrain(targetR, -200, 200);

  speedL = map(targetL, -200, 200, -maxSpeed, maxSpeed);
  speedR = map(targetR, -200, 200, -maxSpeed, maxSpeed);

  writeServoSpeed(SERVO_LEFT_ID, DIR_LEFT * speedL);
  writeServoSpeed(SERVO_RIGHT_ID, DIR_RIGHT * speedR);
  writeServoSpeed(SERVO_TOP_LEFT_ID, DIR_TOP_LEFT * speedL);
  writeServoSpeed(SERVO_TOP_RIGHT_ID, DIR_TOP_RIGHT * speedR);
  updateAndSendBodyCommand();

  if (millis() - lastDisplayUpdate > M5CRAWLER_DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = millis();

    if (millis() - lastRecvTime < M5CRAWLER_AVATAR_LOST_MS) {
      if (abs(speedL) > 500 || abs(speedR) > 500) {
        avatar.setExpression(Expression::Happy);
      } else {
        avatar.setExpression(Expression::Neutral);
      }
    } else {
      avatar.setExpression(Expression::Sad);
    }
  }

  delay(1);
}
