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
  <a href="README_jpn.md">🇯🇵 日本語</a>
</p>

### 报警节点与报警主机：住宅报警的防区、布防、延时、警笛和 PIN，含节点固件和在电脑上测试过的规则（没有在电路板上运行过）

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C368-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**诚实性检查 - 今天真正能运行的部分：** **成熟度：scaffolding。** 目前存在的是**报警核心**（什么是防区、ADC 读取的有线回路如何变成状态、主机的规则：布防、延时、报警、警笛、带锁定的 PIN）以及建立在其上的**节点固件**（设置、网页面板、与服务器的消息、Zigbee 无线电桥接和继电器），可为两种电路板编译；核心、设置、消息以及在真实浏览器中对着模拟节点运行的面板，已在电脑上通过 492,368 项检查。**没有在电路板或真实设施上运行过，没有接过任何防区线路或警笛，节点自己与协调器的 Zigbee 通信（从服务器接管无线电）已编写并对模拟协调器测试过，但从未对真实的协调器或传感器运行过，这也不是经过认证的报警系统。**

---

## 🎯 概述

* **防区：** 四种类型 - *即时*（门、窗）、*进入*（进门用的门：触发进入延时）、*内部*（移动探测：仅在“离家”时）和*全天*（烟雾、燃气、紧急按钮：无论布防与否）。防区可以是有线回路或 Zigbee 传感器。
* **有线回路：** 带终端电阻的触点，以电压方式读取，一个引脚给出四种状态 - 正常、触发，以及线路短路或剪断的防拆。阈值是设置项；去抖器避免抖动的触点引发报警。
* **主机：** *离家*和*在家*两种布防，不把离开的人当作入侵者的退出延时，进入延时，需要正确撤防才会解除的**锁定报警**，一段时间后自动停止的警笛，强制布防时旁路已打开的防区（响着的烟雾探测器永远不会被旁路），以及最近一百条事件的日志。
* **PIN：** 布防不需要，撤防和消音需要；输错的 PIN 会被计数，几次之后一段时间内一切都会被拒绝，连正确的 PIN 也不例外。
* **计划：** 在电气节点电路板上的网关（Zigbee 无线电、有线防区、警笛继电器），与 ARMOR-SERVER 配合，并在**服务器离线时继续守护**：主机通过 ARMOR-ELECTRICAL 的 Zigbee 桥接从服务器手中接管无线电。参见[设计](docs/DESIGN.md)和[接管](docs/FAILOVER.md)。
* **节点：** ESP32-S3 固件（PoE 以太网或 Wi-Fi），最多八个有线防区，警笛接在继电器上，所有节点通用的七种语言网页面板，借给 Zigbee2MQTT 的 Zigbee 无线电，以及消息 `armor/alarm/<节点>/state|command|result`；只有三个相互独立的开关都允许时，服务器才能布防和撤防。参见[固件](docs/NODE_FIRMWARE.md)、[消息](docs/ALARM_MESSAGES.md)和[硬件](docs/HARDWARE.md)。
* **接管无线电：** 服务器的 Zigbee2MQTT 缺席设定时间后，节点自行启动协调器（ZNP），读取指定了传感器的防区（IAS Zone）并控制 Zigbee 警笛（IAS WD），客户端一回来就交还。**尚未完成：** 真实的协调器或传感器、硬件，以及在真实设施上的任何测试。

## 📂 仓库结构

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

## 🛠️ 开发环境

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 492,368 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

参见[设计](docs/DESIGN.md)、[主机如何接替服务器](docs/FAILOVER.md)和[安全说明](docs/SAFETY.md)。

## 🔗 相关项目

**A.R.M.O.R.**（Autonomous Radar & Multimodal Observation Range）是由若干独立仓库组成的周界安防系统。每个仓库都有自己的版本、测试和 README；家族成员如下：

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - 消息契约、验证器、一致性向量和生成的类型
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - 适用于 ESP32-S3 的现场节点固件，带三个雷达和自带网页面板
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - 太阳能逆变器与电池的协议，以及网关节点的消息
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - 电气节点：电表、电网读数消息和开关规则
* **ARMOR-ALARM** (本仓库) - 报警节点与报警主机：防区、布防、延时、警笛和 PIN，有无服务器均可
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - 报警节点与报警主机：防区、布防、延时、警笛和 PIN，有无服务器均可
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - 触摸面板：墙面屏幕上的系统状态、布防与确认，以及语音助手的所在
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - 本地网络：其设备、互联网以及变化
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - 中央协调器：遥测、报警、设备、太阳能读数和摄像头
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - 网页控制台：摄像头、雷达、报警、太阳能和 2D/3D 场地设计器
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - 带实时 2D/3D 雷达的 Android 操作员客户端
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - 会解释决策且从不执行动作的视觉推理策略
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
