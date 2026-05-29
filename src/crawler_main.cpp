#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Avatar.h>
#include "SCServo.h" // libフォルダのローカルライブラリ

using namespace m5avatar;
Avatar avatar;

// ESP-NOW用データ構造体
struct ControlData {
  int x;
  int y;
  int maxSpd;
};
ControlData incomingData;

SMS_STS sts;
const int SERVO_LEFT_ID = 1;
const int SERVO_RIGHT_ID = 2;
const int SERVO_TOP_LEFT_ID = 3;
const int SERVO_TOP_RIGHT_ID = 4;
const int RX_PIN = 2; // G1 -> URT-1 TX
const int TX_PIN = 1; // G2 -> URT-1 RX

int joyX = 0;
int joyY = 0;
int speedL = 0;
int speedR = 0;
int maxSpeed = 2000;
unsigned long lastDisplayUpdate = 0;
unsigned long lastRecvTime = 0;

// ESP-NOW受信コールバック
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming, int len) {
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incoming, int len) {
#endif
  if (len == sizeof(ControlData)) {
    memcpy(&incomingData, incoming, sizeof(incomingData));
    joyX = incomingData.x;
    joyY = incomingData.y;
    maxSpeed = incomingData.maxSpd;
    lastRecvTime = millis();
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  // M5Dialの向きに合わせて回転
  M5.Display.setRotation(2);

  // Avatar初期化
  avatar.init();
  // 240x240の画面に合わせてサイズと位置を調整
  // デフォルト(320x240)のセンター160を、240のセンター120に持ってくるため左に40ずらす
  avatar.setScale(0.75);
  avatar.setPosition(0, -40);
Serial1.begin(1000000, SERIAL_8N1, RX_PIN, TX_PIN);
sts.pSerial = &Serial1;
sts.IOTimeOut = 10; // タイムアウトを少し延ばす

// 各サーボの初期設定を確実に行う
int ids[] = {SERVO_LEFT_ID, SERVO_RIGHT_ID, SERVO_TOP_LEFT_ID, SERVO_TOP_RIGHT_ID};
for (int id : ids) {
  sts.unLockEprom(id);      // EPROMロック解除
  sts.WheelMode(id);        // ホイールモード設定
  sts.EnableTorque(id, 1);  // トルク有効化
  delay(50);                // 連続送信による取りこぼし防止
}
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
  }
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  M5.update();

  if (millis() - lastRecvTime > 1000) {
    joyX = 0;
    joyY = 0;
  }

  int targetL = joyY - joyX;
  int targetR = joyY + joyX;

  speedL = map(targetL, -200, 200, -maxSpeed, maxSpeed);
  speedR = map(targetR, -200, 200, -maxSpeed, maxSpeed);
  speedR = -speedR; 

  sts.WriteSpe(SERVO_LEFT_ID, speedL, 0);
  sts.WriteSpe(SERVO_RIGHT_ID, speedR, 0);
  sts.WriteSpe(SERVO_TOP_LEFT_ID, -speedL, 0);
  sts.WriteSpe(SERVO_TOP_RIGHT_ID, -speedR, 0);

  // --- 表情管理 (200ms間隔) ---
  if (millis() - lastDisplayUpdate > 200) {
    lastDisplayUpdate = millis();
    
    if (millis() - lastRecvTime < 500) {
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
