# 05. 通信プロトコル仕様

## 1. 通信方式

| 項目 | 仕様 |
| --- | --- |
| 無線方式 | ESP-NOW |
| Wi-Fi モード | `WIFI_STA` |
| チャンネル | 1 |
| 送信先 | `include/ControlProtocol.h` の `M5CRAWLER_CRAWLER_MAC` で指定。未設定時はブロードキャスト |
| 暗号化 | なし |
| 送信周期 | 約 100ms |
| 受信側フェイルセーフ | 1000ms 無受信で停止 |

共通プロトコル定義は `include/ControlProtocol.h` に集約しています。

## 2. 初期化手順

両ファームウェアとも Wi-Fi チャンネルを 1 に固定しています。

```cpp
WiFi.mode(WIFI_STA);
WiFi.disconnect();
esp_wifi_set_promiscuous(true);
esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
esp_wifi_set_promiscuous(false);
```

その後、ESP-NOW を初期化します。

```cpp
esp_now_init();
```

コントローラ側は送信用 peer としてブロードキャストアドレスを登録します。

```cpp
esp_now_peer_info_t peerInfo = {};
memcpy(peerInfo.peer_addr, broadcastAddress, 6);
peerInfo.channel = 1;
peerInfo.encrypt = false;
esp_now_add_peer(&peerInfo);
```

## 3. ペイロード構造

現在の送受信データ構造体は以下です。

```cpp
struct __attribute__((packed)) ControlData {
  uint16_t magic;
  uint8_t version;
  uint8_t size;
  uint16_t sequence;
  int16_t x;
  int16_t y;
  int16_t maxSpd;
  uint8_t checksum;
};
```

| フィールド | 型 | 想定範囲 | 説明 |
| --- | --- | ---: | --- |
| `magic` | `uint16_t` | `0x354D` | M5Crawler 用データ識別子 |
| `version` | `uint8_t` | `1` | プロトコルバージョン |
| `size` | `uint8_t` | `sizeof(ControlData)` | 受信側サイズ検証用 |
| `sequence` | `uint16_t` | `0..65535` | 送信ごとに増加するシーケンス番号 |
| `x` | `int16_t` | `-100..100` | 左右旋回操作量 |
| `y` | `int16_t` | `-100..100` | 前後進操作量 |
| `maxSpd` | `int16_t` | `1000..6000` | サーボ速度上限 |
| `checksum` | `uint8_t` | - | 最終バイトを除く全バイト和の bit 反転 |

## 4. バイナリ互換性

現在は固定長型を使用し、`packed` 指定で構造体サイズを固定しています。
ペイロードサイズは 13 バイトです。

```text
offset  size  field
0       2     magic
2       1     version
3       1     size
4       2     sequence
6       2     x
8       2     y
10      2     maxSpd
12      1     checksum
```

受信側は `magic`, `version`, `size`, `checksum`, `x/y/maxSpd` の範囲を検証します。

## 4.1 MAC アドレス設定

`include/ControlProtocol.h` の以下を実機の STA MAC アドレスへ変更すると、MAC アドレス指定の
ピアツーピア通信になります。

```cpp
#define M5CRAWLER_CONTROLLER_MAC {0x24, 0x58, 0x7C, 0xAA, 0xBB, 0xCC}
#define M5CRAWLER_CRAWLER_MAC    {0x24, 0x58, 0x7C, 0xDD, 0xEE, 0xFF}
```

初期値の `FF:FF:FF:FF:FF:FF` は未設定を意味します。

- コントローラ側 `M5CRAWLER_CRAWLER_MAC` が未設定の場合: ブロードキャスト送信
- クローラ側 `M5CRAWLER_CONTROLLER_MAC` が未設定の場合: 任意の送信元を受信
- 実 MAC 指定時: コントローラはユニキャスト送信、クローラは送信元 MAC を検証

両ファームウェアは起動時に自分の STA MAC アドレスを画面表示します。

## 5. 値の意味

### 5.1 `x`

| 値 | 意味 |
| ---: | --- |
| `-100` | 左方向最大操作 |
| `0` | 左右ニュートラル |
| `100` | 右方向最大操作 |

### 5.2 `y`

| 値 | 意味 |
| ---: | --- |
| `-100` | 後退最大操作 |
| `0` | 前後ニュートラル |
| `100` | 前進最大操作 |

実際のサーボ回転方向はクローラ側の取り付け・符号反転に依存します。

### 5.3 `maxSpd`

`maxSpd` はクローラ側で次式のマッピング上限値として使います。

```cpp
speed = map(target, -200, 200, -maxSpeed, maxSpeed);
```

## 6. 送信側エラー処理

送信側は `esp_now_send()` の戻り値を画面表示します。

| 戻り値 | 表示 |
| --- | --- |
| `ESP_OK` | `Send: SUCCESS` |
| その他 | `Send: ERROR` |

送信コールバック `OnDataSent` は登録されていますが、現状では処理は空です。

## 7. 受信側エラー処理

受信側は受信長が `sizeof(ControlData)` と一致する場合のみ採用します。
長さが一致しないデータは無視されます。

送信元 MAC アドレスや値範囲の検証は現状実装されていません。

## 8. 複数台運用時の注意

現状はブロードキャスト送信・送信元未検証のため、同じチャンネル上に同形式の送信機が複数存在すると、
意図しない送信機の操作データを受信する可能性があります。

複数台運用や安全性を高める場合は以下を検討してください。

- 送信先をクローラ側 MAC アドレスに固定する。
- 受信側で送信元 MAC アドレスを検証する。
- ペイロードに `robot_id` または `pairing_id` を追加する。
- チェックサムやシーケンス番号を追加する。

## 9. BodyCommand（クローラ → Stack-chan）

Stack-chan(CoreS3) 連携用に、クローラから Stack-chan へ送る `BodyCommand` を追加しています。
詳細は [09_stackchan_integration.md](09_stackchan_integration.md) を参照してください。

送信元:

- `src/crawler_main.cpp`

受信側:

- `src/stackchan_main.cpp`

送信先 MAC:

- `M5CRAWLER_STACKCHAN_MAC`

未設定時はブロードキャスト送信です。
