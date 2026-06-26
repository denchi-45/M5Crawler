# 09. Stack-chan(CoreS3) 連携仕様

## 1. 目的

クローラ車体の上に M5 公式版 Stack-chan（CoreS3）を搭載し、クローラの進行方向に合わせて
Stack-chan の体（向き）を動かします。

重要な設計方針:

- コントローラは従来通りクローラへ操作データを送る。
- クローラ（StampS3）が進行方向を計算する。
- クローラから Stack-chan（CoreS3）へ ESP-NOW で「体の向き指示」を送る。
- Stack-chan 側は受信した指示を適用する。
- クローラから Stack-chan のサーボを直接配線制御しない。

## 2. 構成

```text
M5StickC Plus2 Controller
  └─ ESP-NOW ControlData
       ↓
M5Stack StampS3 Crawler
  ├─ クローラ用4サーボを制御
  └─ 進行方向を計算
       └─ ESP-NOW BodyCommand
            ↓
M5Stack CoreS3 Stack-chan
  └─ 受信した BodyCommand に従って体を向ける
```

## 3. MAC アドレス設定

`include/ControlProtocol.h` に Stack-chan の MAC アドレスを追加しています。

```cpp
#ifndef M5CRAWLER_STACKCHAN_MAC
#define M5CRAWLER_STACKCHAN_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#endif
```

初期値 `FF:FF:FF:FF:FF:FF` の場合、クローラは Stack-chan へブロードキャスト送信します。
実機 MAC を設定するとユニキャスト送信になります。

## 4. BodyCommand

クローラから Stack-chan へ送る構造体です。

```cpp
struct __attribute__((packed)) BodyCommand {
  uint16_t magic;
  uint8_t version;
  uint8_t size;
  uint16_t sequence;
  int16_t yaw;
  int16_t pitch;
  uint8_t moving;
  uint8_t checksum;
};
```

| フィールド | 意味 |
| --- | --- |
| `yaw` | 体の左右向き。単位は度。正の値は右方向 |
| `pitch` | 上下向き。単位は度。現状は通常 0 |
| `moving` | クローラが移動中なら 1、停止中なら 0 |
| `sequence` | 送信ごとに増える番号 |
| `checksum` | 受信検証用 |

## 5. 進行方向計算

クローラ側は、受信済みの `joyX`, `joyY` から Stack-chan の向き `yaw` を連続的に計算します。

```cpp
yaw = atan2(joyX, abs(joyY)) * 180 / PI;
```

| 操作 | yaw |
| --- | ---: |
| 前進 | 0 |
| 前進しながら右旋回 | 0..+90 |
| 前進しながら左旋回 | 0..-90 |
| 右方向 | +90 |
| 左方向 | -90 |
| 後退直進 | 0 |

停止中は `moving = 0`, `yaw = 0` を送ります。

この計算では、前進/後退直進では正面を向き、旋回成分が強くなるほど横を向きます。
その場回転では真横（±90度）を向きます。
後退時に後ろを向かせる動きが必要になった場合は、`calculateStackchanYaw()` を拡張します。

## 6. 送信周期

`M5CRAWLER_BODY_SEND_INTERVAL_MS` に従い、現状は約 100ms ごとに送信します。

## 7. Stack-chan 側ファームウェア

追加ファイル:

- `src/stackchan_main.cpp`

追加 PlatformIO 環境:

- `m5stack-cores3-stackchan`

Stack-chan 側は ESP-NOW で `BodyCommand` を受信し、受信内容を公式 `M5StackChan` BSP の
`M5StackChan.Motion` へ渡します。

現状の変換:

```cpp
servoYaw = yawDeg * 10;
servoPitch = pitchDeg * 10;
```

受信が途切れた場合は `goHome()` 相当のニュートラル姿勢へ戻します。

## 8. 注意点

- `m5stack-cores3-stackchan` 環境は `m5stack-cores3` board 定義が必要なため、既存の
  `espressif32@6.3.1` ではなく Stack-chan 環境のみ `espressif32` 最新系を使います。
- Stack-chan 公式 BSP は依存が大きく、初回ビルドに時間がかかる可能性があります。
- 実機で yaw の左右方向が逆の場合は、Stack-chan 側の `stackchanYawToServoUnit()` で符号を反転してください。
- 後退時に ±180 度まで向けるか、左右 90 度までに制限するかは運用に応じて調整してください。
