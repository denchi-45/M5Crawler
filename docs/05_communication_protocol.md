# 05. 通信プロトコル仕様

## 1. 通信方式

| 項目 | 仕様 |
| --- | --- |
| 無線方式 | ESP-NOW |
| Wi-Fi モード | `WIFI_STA` |
| チャンネル | 1 |
| 送信先 | ブロードキャスト `FF:FF:FF:FF:FF:FF` |
| 暗号化 | なし |
| 送信周期 | 約 100ms |
| 受信側フェイルセーフ | 1000ms 無受信で停止 |

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
struct ControlData {
  int x;
  int y;
  int maxSpd;
};
```

| フィールド | 型 | 想定範囲 | 説明 |
| --- | --- | ---: | --- |
| `x` | `int` | `-100..100` | 左右旋回操作量 |
| `y` | `int` | `-100..100` | 前後進操作量 |
| `maxSpd` | `int` | `1000..6000` | サーボ速度上限 |

## 4. バイナリ互換性

現在は C/C++ の `int` をそのまま送信しています。
ESP32 Arduino 環境では通常 `int` は 32bit であり、ペイロードサイズは通常 12 バイトです。

```text
offset  size  field
0       4     x
4       4     y
8       4     maxSpd
```

ただし C/C++ 構造体をそのまま無線送信する方式には以下の注意があります。

- 別アーキテクチャへ移植した場合、`int` サイズが変わる可能性がある。
- 構造体アライメントやエンディアンの前提が暗黙である。
- バージョン番号、チェックサム、マジック値がないため、異なる形式のデータを検出しづらい。

将来的には以下のような固定長型への変更を推奨します。

```cpp
struct ControlDataV1 {
  int16_t x;
  int16_t y;
  int16_t maxSpd;
  uint8_t version;
  uint8_t checksum;
};
```

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
