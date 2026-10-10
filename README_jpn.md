<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-ALARM

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  🇯🇵 <b>日本語</b>
</p>

### 警報ノードと警報盤：住宅の警報の警戒区域、警戒セット、遅延、サイレン、PIN を、ノードのファームウェアとコンピューターでテストしたルールを含みます（基板では動作していません）

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C368-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**正直さのチェック - 今日動いているもの：** **成熟度：scaffolding。** 存在するのは**警報のコア**（警戒区域とは何か、ADC が読む配線ループがどう状態になるか、警報盤のルール：警戒セット、遅延、警報、サイレン、ロックアウト付き PIN）と、その上に作られた**ノードのファームウェア**（設定、Web パネル、サーバーとのメッセージ、Zigbee 無線のブリッジとリレー）で、両方の基板向けにビルドできます。コア、設定、メッセージ、そして模擬ノードに対して実ブラウザで動かしたパネルは、コンピューターで 492,368 件のチェックを通過しています。**基板や実際の設備では動作しておらず、区域の配線もサイレンの接続もしておらず、ノード自身がコーディネーターと行う Zigbee 通信（サーバーから無線を引き継ぐ）は作成済みで模擬コーディネーターに対してテスト済みですが、実機のコーディネーターやセンサーでは一度も動作しておらず、、認証された警報システムではありません。**

---

## 🎯 概要

* **警戒区域：** 4 種類 - *即時*（ドア、窓）、*進入*（入る側のドア：進入遅延を開始）、*室内*（動き：「外出」のときだけ）、*常時*（煙、ガス、非常ボタン：警戒中でなくても）。区域は配線ループでも Zigbee センサーでもかまいません。
* **配線ループ：** 終端抵抗付きの接点を電圧として読み、1 本のピンで 4 つの状態 - 正常、作動、そして配線のショートや断線による改ざん - を得ます。しきい値は設定で、チャタリング除去により、バウンドする接点が警報を鳴らすことはありません。
* **警報盤：** *外出*と*在宅*の警戒セット、出て行く人を侵入者扱いしない退出遅延、進入遅延、正しい解除まで**保持される警報**、一定時間で自動的に止まるサイレン、強制時に開いている区域をバイパス（鳴っている煙感知器は決してバイパスしません）、そして直近 100 件のイベントログ。
* **PIN：** 警戒セットには不要、解除と消音には必要です。誤った PIN は数えられ、数回で一定時間すべて拒否されます（正しい PIN も）。
* **計画：** 電気ノードの基板上のゲートウェイ（Zigbee 無線、配線区域、サイレン用リレー）。ARMOR-SERVER と連携し、**サーバーが不在でも警戒を続けます**：警報盤が ARMOR-ELECTRICAL の Zigbee ブリッジ経由でサーバーから無線を引き継ぎます。[設計](docs/DESIGN.md)と[引き継ぎ](docs/FAILOVER.md)を参照。
* **ノード：** ESP32-S3 用ファームウェア（PoE イーサネットまたは Wi-Fi）。最大 8 つの配線区域、リレーに接続したサイレン、全ノード共通の 7 言語の Web パネル、Zigbee2MQTT に貸し出す Zigbee 無線、そしてメッセージ `armor/alarm/<ノード>/state|command|result`。サーバーが警戒セット・解除できるのは、独立した 3 つのスイッチがすべて許可した場合だけです。[ファームウェア](docs/NODE_FIRMWARE.md)、[メッセージ](docs/ALARM_MESSAGES.md)、[ハードウェア](docs/HARDWARE.md)を参照。
* **無線の引き継ぎ：** サーバーの Zigbee2MQTT が設定時間いないと、ノードが自分でコーディネーターを起動し（ZNP）、センサーを指定した区域を読み取り（IAS Zone）、Zigbee サイレンを制御し（IAS WD）、クライアントが戻った瞬間に手放します。**まだ：** 実機のコーディネーターやセンサー、ハードウェア、実際の設備でのテスト。

## 📂 リポジトリの構成

```text
ARMOR-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (seven languages)
├── tests/   test_alarm (the rules), test_node (settings, messages, runner), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ 開発環境

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 492,368 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

[設計](docs/DESIGN.md)、[警報盤がサーバーから引き継ぐ方法](docs/FAILOVER.md)、[安全に関する注意](docs/SAFETY.md)を参照してください。

## 🔗 関連プロジェクト

**A.R.M.O.R.**（Autonomous Radar & Multimodal Observation Range）は、独立したリポジトリで構成される周辺警備システムです。それぞれに独自のバージョン、テスト、README があります。ファミリーは次のとおりです：

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - メッセージ契約、検証器、適合性ベクトル、生成された型
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - ESP32-S3 用フィールドノードのファームウェア。レーダー 3 基と独自の Web パネル付き
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - 太陽光インバーターとバッテリーのプロトコル、およびゲートウェイノードのメッセージ
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - 電気ノード：電力量計、電力網の計測メッセージ、開閉のルール
* **ARMOR-ALARM** (このリポジトリ) - 警報ノードと警報盤：警戒区域、警戒セット、遅延、サイレン、PIN。サーバーがあってもなくても
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - 警報ノードと警報盤：警戒区域、警戒セット、遅延、サイレン、PIN。サーバーがあってもなくても
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - タッチパネル：壁面ディスプレイでのシステム状態表示、警戒・確認操作、音声アシスタントの拠点
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - ローカルネットワーク：機器、インターネット、そして変化
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - 中央コーディネーター：テレメトリ、アラーム、デバイス、太陽光の測定値、カメラ
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web コンソール：カメラ、レーダー、アラーム、太陽光発電、2D/3D サイト設計
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - リアルタイム 2D/3D レーダー付きの Android オペレータークライアント
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - 判断を説明し、決して動作しない視覚推論ポリシー
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - 偽造できない確認を備えたオフライン音声インテント
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - 筐体、電子部品、ベンチ受け入れマトリクス
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - デプロイ、CM5 テストベンチ、バックアップ、TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - 再現可能な故障を備えたオフラインのテレメトリシミュレーター
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - エコシステム自身のリポジトリを検出し、インストールし、更新する
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - アーキテクチャ、セキュリティ基準、機能マトリクス

## 📚 ドキュメントとコミュニティ

詳しくは：

* [機能マトリクス：実証済みのものとそうでないもの](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [プロジェクト一覧：バージョンとリポジトリ間の依存関係](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [このリポジトリの変更履歴](CHANGELOG.md)
* [ライセンス（GPL-3.0-or-later）](LICENSE)
* 質問・提案・報告：electrohobby3d@gmail.com

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 ライセンス

GPL-3.0-or-later - [LICENSE](LICENSE) を参照。
