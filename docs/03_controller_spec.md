# 03. コントローラ仕様

対象ファイル: `src/controller_main.cpp`

## 1. 役割

コントローラは MiniJoyC のジョイスティック入力を読み取り、補正・正規化した操作データを
ESP-NOW でクローラ側へ送信します。また、M5 本体の A ボタンで速度上限を切り替え、
現在の状態を画面および MiniJoyC RGB LED に表示します。

## 2. 主要変数

| 変数 | 型 | 初期値 | 説明 |
| --- | --- | ---: | --- |
| `data` | `ControlData` | 未初期化（グローバル） | 送信する操作データ |
| `joyc` | `M5HatMiniJoyC` | - | MiniJoyC ドライバ |
| `maxSpeed` | `int` | 2000 | サーボ速度上限。A ボタンで変更 |
| `lastSendTime` | `unsigned long` | 0 | 直近送信時刻 |
| `joyDetected` | `bool` | false | MiniJoyC 検出結果 |
| `offsetX` | `int8_t` | 0 | 起動時 X 軸ニュートラル値 |
| `offsetY` | `int8_t` | 0 | 起動時 Y 軸ニュートラル値 |

## 3. 起動シーケンス

```text
setup()
  ├─ M5.begin()
  ├─ 画面初期化
  ├─ MiniJoyC begin(Wire, 0x54, SDA=0, SCL=26, 100kHz)
  │   ├─ 成功:
  │   │   ├─ joyDetected = true
  │   │   ├─ 10サンプル読み取り
  │   │   └─ offsetX / offsetY に平均値を保存
  │   └─ 失敗:
  │       └─ joyDetected = false
  ├─ WiFi.mode(WIFI_STA)
  ├─ WiFi.disconnect()
  ├─ Wi-Fi CH1 固定
  ├─ esp_now_init()
  ├─ 送信コールバック登録
  ├─ ブロードキャスト peer 追加
  └─ Setup Done 表示
```

## 4. ジョイスティック読み取り・補正

### 4.1 入力値

MiniJoyC から 8bit 位置値を読みます。

```cpp
int8_t pos_x = joyc.getPOSValue(POS_X, _8bit);
int8_t pos_y = joyc.getPOSValue(POS_Y, _8bit);
```

ドライバの戻り値は `uint16_t` ですが、呼び出し側では `int8_t` に代入しています。
このため 8bit 値は `-128..127` として扱われます。

### 4.2 起動時オフセット補正

起動時に取得したニュートラル値を差し引きます。

```cpp
corrected_x = pos_x - offsetX;
corrected_y = pos_y - offsetY;
```

### 4.3 入力範囲制限

```cpp
corrected_x = constrain(corrected_x, -128, 127);
corrected_y = constrain(corrected_y, -128, 127);
```

### 4.4 操作用データへのマッピング

```cpp
data.x = map(corrected_x, -128, 127, -100, 100);
data.y = map(corrected_y, -128, 127, 100, -100);
```

| 軸 | 入力 | 出力 | 備考 |
| --- | --- | --- | --- |
| X | `-128..127` | `-100..100` | 左右旋回 |
| Y | `-128..127` | `100..-100` | 符号方向を反転 |

### 4.5 デッドゾーン

```cpp
if (abs(data.x) < 5) data.x = 0;
if (abs(data.y) < 5) data.y = 0;
```

絶対値 5 未満はゼロとして扱います。

## 5. 速度上限切替

M5 本体 A ボタン押下で `maxSpeed` を 1000 ずつ増加します。

```cpp
maxSpeed += 1000;
if (maxSpeed > 6000) maxSpeed = 1000;
```

| 押下回数イメージ | `maxSpeed` |
| ---: | ---: |
| 起動時 | 2000 |
| 1 | 3000 |
| 2 | 4000 |
| 3 | 5000 |
| 4 | 6000 |
| 5 | 1000 |
| 6 | 2000 |

`data.maxSpd` に常に現在値を反映します。

## 6. ESP-NOW 送信

### 6.1 送信先

```cpp
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
```

現状はブロードキャスト送信です。

### 6.2 送信周期

100ms ごとに送信します。

```cpp
if (millis() - lastSendTime > 100) {
    esp_now_send(...);
}
```

### 6.3 送信データ

```cpp
struct ControlData {
  int x;
  int y;
  int maxSpd;
};
```

詳細は [05_communication_protocol.md](05_communication_protocol.md) を参照してください。

## 7. MiniJoyC RGB LED 表示

ジョイスティック操作量に応じて MiniJoyC の RGB LED を点灯します。

```cpp
uint8_t r = abs(data.y) * 2;
uint8_t b = abs(data.x) * 2;
joyc.setLEDColor((r << 16) | b);
```

| 操作 | LED |
| --- | --- |
| 前後方向操作 | 赤成分が増加 |
| 左右方向操作 | 青成分が増加 |
| ニュートラル | 消灯に近い |

## 8. エラー時挙動

| 状況 | 挙動 |
| --- | --- |
| MiniJoyC 未検出 | `joyDetected = false`、画面に `JOYSTICK ERROR` 表示 |
| ESP-NOW 初期化失敗 | 画面表示後 `setup()` を return。以後 loop は継続するが ESP-NOW 送信は失敗する可能性あり |
| peer 追加失敗 | 画面に `Failed to add peer` 表示。以後も loop 継続 |
| 送信失敗 | 通常画面で `Send: ERROR` 表示 |

## 9. 今後変更時の注意

- `ControlData` の構造体定義はコントローラ側・クローラ側で完全一致させる必要があります。
- `int` サイズは ESP32 Arduino 環境では通常 32bit ですが、他環境へ移植する場合は固定長型への変更を推奨します。
- ジョイスティックの符号方向を変更する場合、クローラ側の差動計算およびサーボ符号反転との整合を確認してください。
