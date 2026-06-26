# 07. ビルド・書き込み手順

## 1. 前提

このプロジェクトは PlatformIO 用 Arduino プロジェクトです。

```ini
platform = espressif32@6.3.1
framework = arduino
```

1 つのプロジェクト内に 2 種類のファームウェアがあり、PlatformIO の環境を指定してビルドします。

現在は Stack-chan(CoreS3) 連携追加により、3 種類のファームウェアがあります。

## 2. PlatformIO 環境

### 2.1 クローラ側: `m5stack-stamps3`

```ini
[env:m5stack-stamps3]
platform = espressif32@6.3.1
board = m5stack-stamps3
framework = arduino
lib_deps =
    m5stack/M5Unified@^0.2.14
    meganetaaan/M5Stack-Avatar@^0.10.0
lib_ldf_mode = deep+
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D SCK=13
    -D MISO=11
    -D MOSI=15
    -D SS=12
upload_speed = 115200
build_src_filter = +<crawler_main.cpp>
```

ビルド対象:

- `src/crawler_main.cpp`
- `lib/SCServo/*`

### 2.2 コントローラ側: `m5stick-c-plus2`

```ini
[env:m5stick-c-plus2]
platform = espressif32@6.3.1
board = m5stick-c
framework = arduino
lib_deps =
    m5stack/M5Unified@^0.2.14
build_src_filter = +<controller_main.cpp> +<M5HatMiniJoyC.cpp>
```

ビルド対象:

- `src/controller_main.cpp`
- `src/M5HatMiniJoyC.cpp`
- `include/M5HatMiniJoyC.h`

> 注意: 環境名は `m5stick-c-plus2` ですが、`board = m5stick-c` です。
> M5StickC Plus2 専用ボード定義が必要な場合は今後見直し候補です。

### 2.3 Stack-chan 側: `m5stack-cores3-stackchan`

```ini
[env:m5stack-cores3-stackchan]
platform = espressif32
board = m5stack-cores3
framework = arduino
lib_deps =
    m5stack/M5Unified@^0.2.14
    https://github.com/m5stack/StackChan-BSP.git
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
build_src_filter = +<stackchan_main.cpp>
```

ビルド対象:

- `src/stackchan_main.cpp`

Stack-chan 環境は CoreS3 の board 定義が必要なため、既存の `espressif32@6.3.1` ではなく
`espressif32` 最新系を使用します。

## 3. VS Code / PlatformIO での操作

1. VS Code でプロジェクトフォルダを開く。
2. PlatformIO の Project Tasks から対象環境を選ぶ。
3. `Build` でビルド。
4. USB 接続した対象デバイスに対して `Upload` で書き込み。

## 4. CLI での操作例

### 4.1 クローラ側ビルド

```powershell
pio run -e m5stack-stamps3
```

メモリ不足でビルドが失敗する場合は単一ジョブで実行します。

```powershell
pio run -e m5stack-stamps3 -j 1
```

### 4.2 クローラ側書き込み

```powershell
pio run -e m5stack-stamps3 -t upload
```

### 4.3 コントローラ側ビルド

```powershell
pio run -e m5stick-c-plus2
```

メモリ不足でビルドが失敗する場合は単一ジョブで実行します。

```powershell
pio run -e m5stick-c-plus2 -j 1
```

### 4.4 コントローラ側書き込み

```powershell
pio run -e m5stick-c-plus2 -t upload
```

### 4.5 Stack-chan 側ビルド

```powershell
pio run -e m5stack-cores3-stackchan -j 1
```

### 4.6 Stack-chan 側書き込み

```powershell
pio run -e m5stack-cores3-stackchan -t upload
```

## 5. ビルド対象切替の仕組み

`platformio.ini` の `build_src_filter` により、同じ `src/` ディレクトリ内の複数 main ファイルを
環境ごとに切り替えています。

| 環境 | build_src_filter |
| --- | --- |
| `m5stack-stamps3` | `+<crawler_main.cpp>` |
| `m5stick-c-plus2` | `+<controller_main.cpp> +<M5HatMiniJoyC.cpp>` |
| `m5stack-cores3-stackchan` | `+<stackchan_main.cpp>` |

このため、新しい `.cpp` ファイルを追加した場合は、どちらの環境でビルド対象にするかを
`build_src_filter` で明示する必要があります。

## 6. シリアルモニタ

現状コードでは画面表示が中心で、シリアル出力はクローラ側の ESP-NOW 初期化失敗時のみです。

```cpp
Serial.println("Error initializing ESP-NOW");
```

デバッグを強化する場合は、以下のような情報をシリアル出力すると有用です。

- 起動時のデバイス情報
- ESP-NOW 初期化結果
- 受信した `ControlData`
- サーボ初期化結果
- サーボ電圧・温度・電流

## 7. クリーンビルド

依存関係やビルドキャッシュの問題が疑われる場合は、PlatformIO の Clean を実行します。

```powershell
pio run -t clean
```

または環境指定:

```powershell
pio run -e m5stack-stamps3 -t clean
pio run -e m5stick-c-plus2 -t clean
```

## 8. 書き込み時の注意

- コントローラ用ファームウェアをクローラへ、またはその逆へ書き込まないよう環境名を確認してください。
- クローラ側はサーボ電源を別途必要とする可能性があります。書き込み時にサーボが暴走しないよう、必要に応じてサーボ電源を切ってください。
- ESP-NOW は両者が同じチャンネル（現在は 1）で動作している必要があります。
