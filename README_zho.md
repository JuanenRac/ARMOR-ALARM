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
  🇨🇳 <b>简体中文</b> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### 报警节点与报警主机：住宅报警的防区、布防、延时、警笛和 PIN，含节点固件和在电脑上测试过的规则（没有在电路板上运行过）

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-482%2C535-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Version-0.3.6-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**诚实性检查 - 今天真正能运行的部分:** 目前存在的是**报警核心**（什么是防区、ADC 读取的有线回路如何变成状态、主机的规则：布防、延时、报警、警笛、带锁定的 PIN）以及建立在其上的**节点固件**（设置、网页面板、与服务器的消息、Zigbee 无线电的桥接、网络上的 Zigbee 协调器、继电器、蓝牙、签名更新），可为两种板子构建；核心、设置、消息以及在真实浏览器中对照替身节点的面板，在计算机上用 6 个测试程序中的 477,665 项检查进行测试。**没有任何东西在板上或真实安装中运行过，没有接过任何防区，也没有连接过警笛；节点自己与协调器（本地或网络上）的 Zigbee 对话已编写并对照替身协调器测试，但从未对照真实的协调器或传感器运行，而且这不是经过认证的报警系统。**

---

## 📋 速览

| | |
|---|---|
| 版本 | `0.3.6` |
| 成熟度 | 可用 |
| 测试 | 482,535 checks in 7 programs (`ctest`) |
| 技术栈 | C++17 host-tested core; ESP-IDF 5.4 firmware for the ESP32-S3 (PoE Ethernet and Wi-Fi boards) |
| 真实硬件 | 暂无：没有连接板子、防区、警笛或 Zigbee 协调器（仅有计算机测试、构建和浏览器中的面板） |

## 🎯 概述

* **防区：** 四种类型 - *即时*（门、窗）、*进入*（进门用的门：触发进入延时）、*内部*（移动探测：仅在“离家”时）和*全天*（烟雾、燃气、紧急按钮：无论布防与否）。防区可以是有线回路或 Zigbee 传感器。
* **有线回路：** 带终端电阻的触点，以电压方式读取，一个引脚给出四种状态 - 正常、触发，以及线路短路或剪断的防拆。阈值是设置项；去抖器避免抖动的触点引发报警。
* **主机：** *离家*和*在家*两种布防，不把离开的人当作入侵者的退出延时，进入延时，需要正确撤防才会解除的**锁定报警**，一段时间后自动停止的警笛，强制布防时旁路已打开的防区（响着的烟雾探测器永远不会被旁路），以及最近一百条事件的日志。
* **PIN：** 布防不需要，撤防和消音需要；输错的 PIN 会被计数，几次之后一段时间内一切都会被拒绝，连正确的 PIN 也不例外。
* **不撤防也能消音：** 警笛鸣响时，正确的 PIN 可让它立即停止，而系统仍保持布防 - 报警保持锁定并被记录，尚未响过的防区可再次触发警笛，输错的 PIN 与撤防时输错的 PIN 一样计入锁定。
* **计划：** 在电气节点电路板上的网关（Zigbee 无线电、有线防区、警笛继电器），与 ARMOR-SERVER 配合，并在**服务器离线时继续守护**：主机通过 ARMOR-ELECTRICAL 的 Zigbee 桥接从服务器手中接管无线电。参见[设计](docs/DESIGN.md)和[接管](docs/FAILOVER.md)。
* **节点：** ESP32-S3 固件（PoE 以太网或 Wi-Fi），最多八个有线防区，警笛接在继电器上，所有节点通用的八种语言网页面板，借给 Zigbee2MQTT 的 Zigbee 无线电，以及消息 `armor/alarm/<节点>/state|command|result`；只有三个相互独立的开关都允许时，服务器才能布防和撤防。参见[固件](docs/NODE_FIRMWARE.md)、[消息](docs/ALARM_MESSAGES.md)和[硬件](docs/HARDWARE.md)。
* **接管无线电：** 服务器的 Zigbee2MQTT 缺席设定时间后，节点自行启动协调器（ZNP），读取指定了传感器的防区（IAS Zone）并控制 Zigbee 警笛（IAS WD），客户端一回来就交还。**尚未完成：** 真实的协调器或传感器、硬件，以及在真实设施上的任何测试。
* **重启不会让房子撤防：** 模式、正在进行的报警和被旁路的防区保存在闪存中，启动时恢复；被剪断或短路的回路永远不会被旁路；消息经队列发送（与代理不会死锁），读取防区的任务停止时看门狗会重启开发板。
* **签名的无线更新：**节点只会从 GitHub 安装 SHA-256 匹配且带有维护者有效 Ed25519 签名的镜像（公钥在固件中，私钥从不离开维护者）；管理员在面板中手动上传的文件仍然被接受。面板的页面和消息有八种语言。
* **带 PIN 的蓝牙配对：**六位 PIN 由设置码派生，打印在 USB 控制台上并向管理员显示在面板中；手机必须输入它才能写入节点的设置。
* **警笛与重启：**警笛继电器只属于报警系统（代理、中心和面板都不能控制它，失去代理时它也不会断开）；尚未鸣响过的防区会在警笛超时后让它再次鸣响；重启后，撤防状态下的烟雾或紧急报警、已静音的报警和锁定都会恢复；布防时拒绝手动复位；从未发声的 Zigbee 传感器视为丢失。
* **只有家中才能使用面板：**面板默认只响应私有网络（或所有者列出的网络，如 VPN），拒绝会把保存者自己挡在外面的列表，按人和按地址统计错误密码，并可在服务器代理之后工作，从而无需公开节点就能在家外访问。**无法读取的防区不会显示为已闭合：** 如果转换器无法启动，或某回路连续五轮没有读数，该防区会进入*故障*（trouble）状态：它不守护、从不触发警笛、会阻止布防（强制布防时将其排除并记录），服务器会发出警告。
* **可选的与代理加密连接(TLS):** 默认关闭,因此节点仍像以前一样使用未加密的 MQTT。代理页面中的一个开关和一个用于填写本安装自有证书颁发机构证书(PEM,最大 2 KB)的框,会让节点用 `mqtts://` 连接、只信任该机构,并检查代理证书是否为其所到达的地址而签发;不是单个 PEM 证书的内容会被拒绝。已有主机测试覆盖;握手尚未在电路板上运行。
* **布防配置：** 布防命令可以带一个配置（离家、在家、夜间或自定义），为这次布防选择哪些分区守护、哪些分区立即报警或在进入延时后报警、退出和进入延时，以及警笛是否鸣响（静音报警仍会锁定、记录和通知）。它绝不触及烟雾、紧急按钮或防拆分区、PIN、锁定、消音、强制布防或故障分区；配置在重启后保留，不带配置的布防行为与以前完全一致。已通过随机运行对照旧规则测试，并对照共享契约测试。
* **健康报告：** 节点大约每分钟发布一次：已运行多久、上次为何重启、可用内存、连接与 Wi-Fi 信号、连接代理的次数以及记录的最后一个错误（`armor/health/<节点>`，即 ARMOR-COMMON 的 `node_health` 契约）。生成它的代码由所有节点共享并在计算机上测试；尚未在开发板上运行。

## 🖼️ 屏幕截图

在测试台上以 1920×1080 截取，使用示例数据，背后没有真实的安装。

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="概览：节点状态一目了然——网络、代理、固件及其上报内容。" width="100%"><br><sub>概览：节点状态一目了然——网络、代理、固件及其上报内容。</sub></td><td width="50%" valign="top"><img src="images/screenshots/zones.png" alt="防区：主机的每个有线或 Zigbee 防区，包括类型、保持时间和实时状态。" width="100%"><br><sub>防区：主机的每个有线或 Zigbee 防区，包括类型、保持时间和实时状态。</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/zigbee.png" alt="Zigbee 无线电：通往协调器模块的串口桥接、允许使用它的客户端以及桥接状态。" width="100%"><br><sub>Zigbee 无线电：通往协调器模块的串口桥接、允许使用它的客户端以及桥接状态。</sub></td><td width="50%" valign="top"><img src="images/screenshots/inputs-outputs.png" alt="继电器和警笛：底板的输出以及每个输出由什么驱动。" width="100%"><br><sub>继电器和警笛：底板的输出以及每个输出由什么驱动。</sub></td></tr>
</table>

## 🧩 在 A.R.M.O.R. 中的角色

ARMOR-ALARM 是报警主机：防区、布防、延时、警笛和 PIN 都在节点自身上决定，因此服务器不在时住宅仍受守护。[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) 显示它，把 Zigbee 传感器绑定到其防区，并在你允许时对它布防和撤防；[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) 的*报警主机*菜单和*报警设计器*以及 [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) 应用可控制它。它的 Zigbee 无线电可以在板上，也可以是网络上的协调器。

## 📂 仓库结构

```text
ARMOR-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), arm_profile (the arming profiles), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (eight languages)
├── tests/   test_alarm (the rules), test_profiles (the arming profiles), test_node (settings, messages, runner), test_mqtt_tls (the optional encrypted link to the broker), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ 开发环境

**环境要求:** a C++17 compiler for the host tests, the ESP-IDF 5.4 container to build the image, an ESP32-S3 board (PoE Ethernet or Wi-Fi)

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 482,535 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

参见[设计](docs/DESIGN.md)、[主机如何接替服务器](docs/FAILOVER.md)和[安全说明](docs/SAFETY.md)。

## 🔗 相关项目

**A.R.M.O.R.**（Autonomous Radar & Multimodal Observation Range）是由若干独立仓库组成的周界安防系统。每个仓库都有自己的版本、测试和 README；家族成员如下：

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - 消息契约、验证器、一致性向量和生成的类型
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - 适用于 ESP32-S3 的现场节点固件，带三个雷达和自带网页面板
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - 太阳能逆变器与电池的协议，以及网关节点的消息
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - 电气节点：电表、电网读数消息和开关规则
* **ARMOR-ALARM** (本仓库) - 报警节点与报警主机：防区、布防、延时、警笛和 PIN，有无服务器均可
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - 触摸面板：墙面屏幕上的系统状态、布防与确认，以及语音助手的所在
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - 本地网络：其设备、互联网以及变化
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - 中央协调器：遥测、报警、设备、太阳能读数和摄像头
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - 网页控制台：摄像头、雷达、报警、太阳能和 2D/3D 场地设计器
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - 带实时 2D/3D 雷达的 Android 操作员客户端
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - 会解释决策且从不执行动作的视觉推理策略
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Jetson Orin NX 上的摄像头区域、遮罩与检测规则
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - 带无法伪造确认的离线语音意图
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - 外壳、电子器件和台架验收矩阵
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - 部署、CM5 测试台、备份与 TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - 带可重复故障的离线遥测模拟器
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - 发现、安装并更新生态系统自身的仓库
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - 架构、安全基线和能力矩阵

## 📚 文档与社区

更多阅读：

* [能力矩阵：哪些已被证实，哪些没有](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [项目目录：版本以及各仓库之间的依赖](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [本仓库的变更记录](CHANGELOG.md)
* [许可证（GPL-3.0-or-later）](LICENSE)
* 问题、想法与反馈：electrohobby3d@gmail.com

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 许可证

GPL-3.0-or-later - 见 [LICENSE](LICENSE)。
