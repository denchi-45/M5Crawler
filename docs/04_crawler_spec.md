# 04. クローラ仕様

対象ファイル: `src/crawler_main.cpp`

## 1. 役割

クローラ側ファームウェアは ESP-NOW で受信した操作データをもとに、左右履帯の速度を計算し、
FEETECH SMS/STS 系シリアルサーボをホイールモードで駆動します。
また、受信状態や走行状態に応じて M5Stack-Avatar の表情を切り替えます。

## 2. 主要変数

| 変数 | 型 | 初期値 | 説明 |
| --- | --- | ---: | --- |
| `incomingData` | `ControlData` | 未初期化（グローバル） | 受信した操作データ |
| `sts` | `SMS_STS` | - | シリアルサーボ制御オブジェクト |
| `joyX` | `int` | 0 | 受信した X 操作量 |
| `joyY` | `int` | 0 | 受信した Y 操作量 |
| `speedL` | `int` | 0 | 左側サーボ速度 |
| `speedR` | `int` | 0 | 右側サーボ速度 |
| `maxSpeed` | `int` | 2000 | 速度上限。受信データで更新 |
| `lastDisplayUpdate` | `unsigned long` | 0 | Avatar 表示更新時刻 |
| `lastRecvTime` | `unsigned long` | 0 | 最終受信時刻 |

## 3. 起動シーケンス

```text
setup()
  ├─ M5.begin()
  ├─ Display rotation = 2
  ├─ avatar.init()
  ├─ avatar.setScale(0.75)
  ├─ avatar.setPosition(0, -40)
  ├─ Serial1.begin(1Mbps, 8N1, RX=2, TX=1)
  ├─ sts.pSerial = &Serial1
  ├─ sts.IOTimeOut = 10
  ├─ サーボ ID 1..4 初期化
  │   ├─ unLockEprom()
  │   ├─ WheelMode()
  │   └─ EnableTorque(1)
  ├─ WiFi.mode(WIFI_STA)
  ├─ WiFi.disconnect()
  ├─ Wi-Fi CH1 固定
  ├─ esp_now_init()
  └─ 受信コールバック登録
```

## 4. ESP-NOW 受信

受信コールバック `OnDataRecv` は Arduino ESP32 コアのバージョン差を吸収するため、
以下のように条件コンパイルされています。

```cpp
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming, int len)
#else
void OnDataRecv(const uint8_t * mac, const uint8_t *incoming, int len)
#endif
```

受信長が `sizeof(ControlData)` と一致する場合のみデータを採用します。

```cpp
if (len == sizeof(ControlData)) {
  memcpy(&incomingData, incoming, sizeof(incomingData));
  joyX = incomingData.x;
  joyY = incomingData.y;
  maxSpeed = incomingData.maxSpd;
  lastRecvTime = millis();
}
```

## 5. フェイルセーフ

最終受信から 1000ms 以上経過した場合、操作量をゼロにします。

```cpp
if (millis() - lastRecvTime > 1000) {
  joyX = 0;
  joyY = 0;
}
```

この結果、速度計算後の `speedL`, `speedR` は 0 になります。
サーボへの速度指令は loop 内で毎回送信されます。

## 6. 差動駆動計算

### 6.1 中間値

```cpp
int targetL = joyY - joyX;
int targetR = joyY + joyX;
```

| 操作 | `joyX` | `joyY` | `targetL` | `targetR` | 意味 |
| --- | ---: | ---: | ---: | ---: | --- |
| 前進 | 0 | 正 | 正 | 正 | 両側前進 |
| 後退 | 0 | 負 | 負 | 負 | 両側後退 |
| 右旋回想定 | 正 | 0 | 負 | 正 | 左右逆回転 |
| 左旋回想定 | 負 | 0 | 正 | 負 | 左右逆回転 |

### 6.2 速度マッピング

```cpp
speedL = map(targetL, -200, 200, -maxSpeed, maxSpeed);
speedR = map(targetR, -200, 200, -maxSpeed, maxSpeed);
speedR = -speedR;
```

`joyX` と `joyY` はそれぞれ `-100..100` のため、`targetL` / `targetR` の理論範囲は `-200..200` です。

右側サーボは物理的取り付け方向が左側と逆である想定のため、`speedR` を反転しています。

### 6.3 サーボ出力

```cpp
sts.WriteSpe(SERVO_LEFT_ID, speedL, 0);
sts.WriteSpe(SERVO_RIGHT_ID, speedR, 0);
sts.WriteSpe(SERVO_TOP_LEFT_ID, -speedL, 0);
sts.WriteSpe(SERVO_TOP_RIGHT_ID, -speedR, 0);
```

| ID | 速度指令 |
| ---: | --- |
| 1 | `speedL` |
| 2 | `speedR` |
| 3 | `-speedL` |
| 4 | `-speedR` |

## 7. Avatar 表示仕様

200ms ごとに表情を更新します。

```cpp
if (millis() - lastRecvTime < 500) {
  if (abs(speedL) > 500 || abs(speedR) > 500) {
    avatar.setExpression(Expression::Happy);
  } else {
    avatar.setExpression(Expression::Neutral);
  }
} else {
  avatar.setExpression(Expression::Sad);
}
```

| 条件 | 表情 |
| --- | --- |
| 500ms 未満に受信あり、かつ左右いずれかの速度絶対値が 500 超 | `Happy` |
| 500ms 未満に受信あり、かつ速度が小さい | `Neutral` |
| 最終受信から 500ms 以上 | `Sad` |

フェイルセーフ停止は 1000ms ですが、表示上は 500ms で `Sad` になります。

## 8. ループ周期

`loop()` 末尾に `delay(1)` があり、概ね 1ms 以上の間隔で繰り返されます。
ただし、サーボ通信時間・M5.update・Avatar 更新などの処理時間により実周期は変動します。

## 9. 今後変更時の注意

- サーボ ID や取り付け方向を変える場合は、`speedR` および上側サーボの符号反転を必ず再確認してください。
- `maxSpeed` はコントローラからの受信値を無検証で採用しています。通信データ破損対策を強化する場合は範囲チェックを追加してください。
- ESP-NOW はブロードキャスト受信で送信元チェックをしていません。複数台運用では送信元 MAC アドレスの検証が必要です。
- 起動時のサーボ初期化に失敗してもエラー表示やリトライはありません。運用上必要なら診断機能を追加してください。
