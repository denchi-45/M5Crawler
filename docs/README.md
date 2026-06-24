# M5Crawler ドキュメント

このディレクトリは **M5Crawler** プロジェクトの仕様・設計を記述したドキュメント群です。
ソースコードの実装状況に基づいて作成しており、変更指示・仕様変更・不具合修正に伴って
更新していく「生きた資料」として運用します。

## ドキュメント一覧

| ファイル | 内容 |
| --- | --- |
| [01_system_overview.md](01_system_overview.md) | システム全体構成・データフロー・用語定義 |
| [02_hardware.md](02_hardware.md) | ハードウェア構成・ピン配置・配線 |
| [03_controller_spec.md](03_controller_spec.md) | コントローラ側（送信機）の仕様 |
| [04_crawler_spec.md](04_crawler_spec.md) | クローラ側（車体）の仕様 |
| [05_communication_protocol.md](05_communication_protocol.md) | ESP-NOW 通信プロトコル仕様 |
| [06_libraries.md](06_libraries.md) | 使用ライブラリ・自作ドライバ仕様 |
| [07_build_and_flash.md](07_build_and_flash.md) | ビルド・書き込み手順（PlatformIO 環境） |
| [08_known_issues.md](08_known_issues.md) | 既知の課題・注意点・改善候補 |

## プロジェクト概要（要約）

M5Crawler は、ジョイスティックを備えた **コントローラ** から **クローラ（無限軌道車）** へ
ESP-NOW で操作データを無線送信し、車体側の 4 個のシリアルサーボ（FEETECH SMS/STS）を
連続回転（ホイール）モードで駆動して走行させる、2 デバイス構成の遠隔操作ロボットです。

- **コントローラ**: M5StickC Plus2 + MiniJoyC (Hat) … `src/controller_main.cpp`
- **クローラ**: M5Stack StampS3（+ ディスプレイ／Avatar 表示）… `src/crawler_main.cpp`
- **通信**: ESP-NOW（Wi-Fi CH1、ブロードキャスト送信）

> 注: 1 つの PlatformIO プロジェクト内に 2 つのファームウェアが同居しており、
> `platformio.ini` の `build_src_filter` でビルド対象を切り替えます（[07](07_build_and_flash.md) 参照）。
