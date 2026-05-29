#include <M5Unified.h>
#include "M5HatMiniJoyC.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// MiniJoyC I2C pins
#define MiniJoyC_SDA 0
#define MiniJoyC_SCL 26

// ★受信機（M5Dial）のMACアドレス
// 現在はブロードキャスト（全端末宛）に設定しています。
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct ControlData {
  int x;
  int y;
  int maxSpd;
};
ControlData data;

M5HatMiniJoyC joyc;
int maxSpeed = 2000;
unsigned long lastSendTime = 0;
bool joyDetected = false;

// キャリブレーション用変数
int8_t offsetX = 0;
int8_t offsetY = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // 送信完了コールバック
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextSize(1);
  M5.Display.println("Initializing MiniJoyC...");

  // MiniJoyCの初期化 (Wireを使用)
  if (joyc.begin(&Wire, MiniJoyC_ADDR, MiniJoyC_SDA, MiniJoyC_SCL, 100000UL)) {
    joyDetected = true;
    M5.Display.setTextColor(GREEN);
    M5.Display.println("MiniJoyC Hat: FOUND");

    // 簡易キャリブレーション：起動時の数値をニュートラルとして記録
    M5.Display.setTextColor(YELLOW);
    M5.Display.println("Calibrating... DO NOT TOUCH JOYSTICK");
    long sumX = 0;
    long sumY = 0;
    const int samples = 10;
    for(int i=0; i<samples; i++) {
      sumX += joyc.getPOSValue(POS_X, _8bit);
      sumY += joyc.getPOSValue(POS_Y, _8bit);
      delay(50);
    }
    offsetX = (int8_t)(sumX / samples);
    offsetY = (int8_t)(sumY / samples);
    M5.Display.printf("Offset: X=%d, Y=%d\n", offsetX, offsetY);
  } else {
    joyDetected = false;
    M5.Display.setTextColor(RED);
    M5.Display.println("MiniJoyC Hat: NOT FOUND");
  }
  M5.Display.setTextColor(WHITE);
// ... Wi-Fi設定などは変更なし ...

  // Wi-Fi & ESP-NOW設定 (成功した設定を維持)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    M5.Display.println("ESP-NOW Init Failed");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    M5.Display.println("Failed to add peer");
  }
  
  M5.Display.println("Setup Done.");
  delay(1000);
}

void loop() {
  M5.update();

  if (joyDetected) {
    // 8bitモードで座標取得 (-128 ~ 127)
    int8_t pos_x = joyc.getPOSValue(POS_X, _8bit);
    int8_t pos_y = joyc.getPOSValue(POS_Y, _8bit);

    // オフセット（零点）補正
    int corrected_x = pos_x - offsetX;
    int corrected_y = pos_y - offsetY;

    // 範囲外に丸める (-128 ~ 127 の範囲に収める)
    corrected_x = constrain(corrected_x, -128, 127);
    corrected_y = constrain(corrected_y, -128, 127);

    // クローラー側の期待値 -100 ~ 100 にマップ
    data.x = map(corrected_x, -128, 127, -100, 100);
    data.y = map(corrected_y, -128, 127, 100, -100);

    // 遊び（デッドゾーン）の設定
    if (abs(data.x) < 5) data.x = 0; // 補正したのでデッドゾーンを少し狭く設定
    if (abs(data.y) < 5) data.y = 0;

    // MiniJoyCのLEDを操作状態に合わせて光らせる（おまけ）
    uint8_t r = abs(data.y) * 2;
    uint8_t b = abs(data.x) * 2;
    joyc.setLEDColor((r << 16) | b);
  }

  // Aボタンで速度切り替え
  if (M5.BtnA.wasPressed()) {
    maxSpeed += 1000;
    if (maxSpeed > 6000) maxSpeed = 1000;
  }
  data.maxSpd = maxSpeed;

  // 100msごとに送信
  if (millis() - lastSendTime > 100) {
    lastSendTime = millis();
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data));
    
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.println("CONTROLLER");
    M5.Display.setTextSize(1);
    
    if (joyDetected) {
      M5.Display.printf("X: %d  Y: %d\n", data.x, data.y);
    } else {
      M5.Display.setTextColor(RED);
      M5.Display.println("JOYSTICK ERROR");
      M5.Display.setTextColor(WHITE);
    }
    
    M5.Display.printf("MaxSpeed: %d\n", data.maxSpd);
    
    if (result == ESP_OK) {
      M5.Display.setTextColor(GREEN);
      M5.Display.println("Send: SUCCESS");
    } else {
      M5.Display.setTextColor(RED);
      M5.Display.println("Send: ERROR");
    }
    M5.Display.setTextColor(WHITE);
    M5.Display.printf("CH: 1 / Mode: Broadcast\n");
  }
}