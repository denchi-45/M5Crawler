# 06. ライブラリ・ドライバ仕様

## 1. PlatformIO 依存ライブラリ

`platformio.ini` で以下のライブラリを使用しています。

### 1.1 `m5stack-stamps3` 環境

| ライブラリ | バージョン指定 | 用途 |
| --- | --- | --- |
| `m5stack/M5Unified` | `^0.2.14` | M5Stack デバイス抽象化、Display、ボタン等 |
| `meganetaaan/M5Stack-Avatar` | `^0.10.0` | クローラ側の顔表示 |

### 1.2 `m5stick-c-plus2` 環境

| ライブラリ | バージョン指定 | 用途 |
| --- | --- | --- |
| `m5stack/M5Unified` | `^0.2.14` | M5StickC Plus2 の初期化、Display、ボタン |

## 2. ローカルライブラリ `SCServo`

ディレクトリ: `lib/SCServo`

FEETECH SMS/STS 系シリアルサーボを制御するライブラリです。
クローラ側では `SCServo.h` を include し、`SMS_STS` クラスを使用しています。

### 2.1 使用している API

| API | 用途 |
| --- | --- |
| `sts.pSerial = &Serial1` | 通信に使用する HardwareSerial を指定 |
| `sts.IOTimeOut = 10` | 通信タイムアウト設定 |
| `sts.unLockEprom(id)` | EPROM ロック解除 |
| `sts.WheelMode(id)` | サーボをホイールモードへ設定 |
| `sts.EnableTorque(id, 1)` | トルク有効化 |
| `sts.WriteSpe(id, speed, 0)` | ホイールモードで速度指令 |

### 2.2 使用していないが利用可能な主な API

`lib/SCServo/SMS_STS.h` には以下のような API もあります。

- `WritePosEx()` - 位置制御
- `SyncWritePosEx()` - 複数サーボ同期位置制御
- `ReadPos()` - 現在位置取得
- `ReadSpeed()` - 現在速度取得
- `ReadVoltage()` - 電圧取得
- `ReadTemper()` - 温度取得
- `ReadCurrent()` - 電流取得

診断機能や安全監視を追加する場合に利用候補です。

## 3. 自作/同梱ドライバ `M5HatMiniJoyC`

対象ファイル:

- `include/M5HatMiniJoyC.h`
- `src/M5HatMiniJoyC.cpp`

MiniJoyC Hat を I2C で制御するためのドライバです。

### 3.1 I2C レジスタ

| レジスタ | アドレス | 用途 |
| --- | ---: | --- |
| `ADC_VALUE_REG` | `0x00` | ADC 値 |
| `POS_VALUE_REG_10_BIT` | `0x10` | 10bit 位置値 |
| `POS_VALUE_REG_8_BIT` | `0x20` | 8bit 位置値 |
| `BUTTON_REG` | `0x30` | ボタン状態 |
| `RGB_LED_REG` | `0x40` | RGB LED |
| `CAL_REG` | `0x50` | キャリブレーション値 |
| `FIRMWARE_VERSION_REG` | `0xFE` | ファームウェアバージョン |
| `I2C_ADDRESS_REG` | `0xFF` | I2C アドレス |

### 3.2 主要 API

| API | 説明 |
| --- | --- |
| `begin(wire, addr, sda, scl, speed)` | I2C 初期化とデバイス存在確認 |
| `getADCValue(index)` | X/Y の ADC 値取得 |
| `getPOSValue(index, bit)` | X/Y の位置値取得（8bit または 10bit） |
| `getButtonStatus()` | MiniJoyC ボタン状態取得 |
| `setLEDColor(rgb888color)` | RGB LED 色設定 |
| `setOneCalValue(index, data)` | キャリブレーション値を 1 項目設定 |
| `setAllCalValue(data)` | キャリブレーション値をまとめて設定 |
| `getCalValue(index)` | キャリブレーション値取得 |
| `setI2CAddress(addr)` | I2C アドレス変更 |
| `getI2CAddress()` | I2C アドレス取得 |
| `getFirmwareVersion()` | ファームウェアバージョン取得 |

### 3.3 現状使用している API

コントローラ側で以下を使用しています。

- `begin()`
- `getPOSValue(POS_X, _8bit)`
- `getPOSValue(POS_Y, _8bit)`
- `setLEDColor()`

### 3.4 注意点

- `getPOSValue()` は `uint16_t` を返しますが、コントローラ側は `int8_t` に代入して符号付き 8bit として扱っています。
- `readBytes()` は `requestFrom()` 後に読み取り可能バイト数を確認していません。I2C エラー時の堅牢性を高める場合は改善候補です。
- `getI2CAddress()` / `getFirmwareVersion()` 内では `_wire` ではなくグローバル `Wire.read()` を使っています。複数 I2C バス利用時は注意が必要です。

## 4. M5Unified

両ファームウェアで以下を使用しています。

| API | 用途 |
| --- | --- |
| `M5.config()` | 初期化設定取得 |
| `M5.begin(cfg)` | M5 デバイス初期化 |
| `M5.update()` | ボタン等の状態更新 |
| `M5.Display.*` | 画面表示 |
| `M5.BtnA.wasPressed()` | A ボタン押下検出（コントローラ側） |

## 5. ESP-IDF / Arduino Wi-Fi API

両ファームウェアで以下を使用しています。

| API | 用途 |
| --- | --- |
| `WiFi.mode(WIFI_STA)` | STA モード設定 |
| `WiFi.disconnect()` | 既存接続切断 |
| `esp_wifi_set_promiscuous()` | チャンネル変更前後の promiscuous 切替 |
| `esp_wifi_set_channel()` | Wi-Fi チャンネル固定 |
| `esp_now_init()` | ESP-NOW 初期化 |
| `esp_now_register_send_cb()` | 送信コールバック登録（コントローラ） |
| `esp_now_add_peer()` | 送信先 peer 登録（コントローラ） |
| `esp_now_send()` | ESP-NOW 送信（コントローラ） |
| `esp_now_register_recv_cb()` | 受信コールバック登録（クローラ） |
