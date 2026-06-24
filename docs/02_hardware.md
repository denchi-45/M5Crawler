# 02. ハードウェア仕様

## 1. 全体構成

| サブシステム | 使用機器 | 用途 |
| --- | --- | --- |
| コントローラ | M5StickC Plus2 | ESP-NOW 送信、画面表示、A ボタン入力 |
| コントローラ入力 | M5Hat MiniJoyC | ジョイスティック X/Y 入力、RGB LED 表示 |
| クローラ制御 | M5Stack StampS3 | ESP-NOW 受信、シリアルサーボ制御、Avatar 表示 |
| アクチュエータ | FEETECH SMS/STS 系シリアルサーボ x4 | 左右履帯の駆動 |

## 2. コントローラ側

### 2.1 MiniJoyC I2C 接続

`src/controller_main.cpp` では以下のピンを使用しています。

| 信号 | GPIO | 定義 |
| --- | ---: | --- |
| SDA | 0 | `MiniJoyC_SDA` |
| SCL | 26 | `MiniJoyC_SCL` |
| I2C アドレス | `0x54` | `MiniJoyC_ADDR` |
| I2C 速度 | 100kHz | `joyc.begin(..., 100000UL)` |

### 2.2 操作入力

| 入力 | 機能 |
| --- | --- |
| MiniJoyC X 軸 | 左右旋回操作 |
| MiniJoyC Y 軸 | 前後進操作 |
| M5 本体 A ボタン | 最大速度 `maxSpeed` の段階切替 |

### 2.3 表示

M5StickC Plus2 の画面に以下を表示します。

- 起動時:
  - MiniJoyC 初期化状態
  - キャリブレーション中メッセージ
  - 起動時オフセット値
  - ESP-NOW 初期化結果
- 通常動作時（約 100ms ごと）:
  - `CONTROLLER`
  - `X`, `Y`
  - `MaxSpeed`
  - 送信成功/失敗
  - `CH: 1 / Mode: Broadcast`

## 3. クローラ側

### 3.1 サーボ UART 接続

`src/crawler_main.cpp` では `Serial1` を 1Mbps で使用します。

| 信号 | GPIO | コメント |
| --- | ---: | --- |
| RX | 2 | `G1 -> URT-1 TX` とコメントあり |
| TX | 1 | `G2 -> URT-1 RX` とコメントあり |
| UART 設定 | 1,000,000 bps / 8N1 | `Serial1.begin(1000000, SERIAL_8N1, RX_PIN, TX_PIN)` |

> 注意: コメント上は `RX_PIN = 2`, `TX_PIN = 1` です。
> 実配線では **M5 側 RX はサーボバス側 TX、M5 側 TX はサーボバス側 RX** に接続してください。

### 3.2 サーボ ID

| サーボ | ID | 定義名 | 出力指令 |
| --- | ---: | --- | --- |
| 左主履帯 | 1 | `SERVO_LEFT_ID` | `speedL` |
| 右主履帯 | 2 | `SERVO_RIGHT_ID` | `speedR` |
| 左補助履帯 | 3 | `SERVO_TOP_LEFT_ID` | `-speedL` |
| 右補助履帯 | 4 | `SERVO_TOP_RIGHT_ID` | `-speedR` |

上側サーボは下側サーボと物理的な向きが逆である想定のため、速度指令の符号を反転しています。

### 3.3 サーボ初期化

起動時に ID 1〜4 の各サーボに対して以下を順に実行します。

1. `sts.unLockEprom(id)` - EPROM ロック解除
2. `sts.WheelMode(id)` - ホイールモード設定
3. `sts.EnableTorque(id, 1)` - トルク有効化
4. `delay(50)` - 連続通信の取りこぼし防止

> 注意: 現状コードでは起動ごとに EPROM ロック解除とホイールモード設定を行っています。
> サーボの不揮発設定への書き込み頻度や、設定変更の必要性は今後確認対象です。

## 4. クローラ側ディスプレイ / Avatar

クローラ側では `M5Stack-Avatar` を使用します。

| 項目 | 値 |
| --- | --- |
| 画面回転 | `M5.Display.setRotation(2)` |
| Avatar scale | `0.75` |
| Avatar position | `(0, -40)` |

表情仕様は [04_crawler_spec.md](04_crawler_spec.md) を参照してください。

## 5. ビルド時 SPI ピン定義（StampS3 環境）

`platformio.ini` の `m5stack-stamps3` 環境では以下のビルドフラグが設定されています。

| マクロ | 値 |
| --- | ---: |
| `SCK` | 13 |
| `MISO` | 11 |
| `MOSI` | 15 |
| `SS` | 12 |

現状のアプリケーションコード内で明示的に SPI 通信は使用していませんが、M5Unified / 表示系ライブラリの
前提設定として残されています。
