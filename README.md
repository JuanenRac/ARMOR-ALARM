<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-NODE-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-NODE-ALARM

<p align="center">
  🇺🇸 <b>English</b> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Alarm node and panel: the zones, the arming, the delays, the siren and the PIN of the house's alarm, with the firmware of the node and the rules tested on a computer (nothing has run on a board)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-482%2C535-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Version-0.4.5-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Honesty check - what runs today:** What exists is the **core of the alarm** (what a zone is, how a wired loop read by the ADC becomes a state, the panel's rules: arming, delays, the alarm, the siren, the PIN with lockout) and the **firmware of the node** built on it (settings, web panel, messages with the server, the Zigbee radio's bridge, a Zigbee coordinator on the network, relays, Bluetooth, signed updates), which builds for both boards; the core, the settings, the messages and the panel in a real browser against a stand-in node are tested on a computer with 486,690 checks in 14 test programs. **Nothing has run on a board or on a real installation, no zone has been wired and no siren connected, the node's own Zigbee conversation with a coordinator (local or on the network) is written and tested against a stand-in coordinator but has never run against a real coordinator or a real sensor, and this is not a certified alarm system.**

---

## 📋 At a glance

| | |
|---|---|
| Version | `0.4.5` |
| Maturity | Functional |
| Tests | 486,690 checks in 14 programs (`ctest`) |
| Stack | C++17 host-tested core; ESP-IDF 5.5.5 firmware for the ESP32-S3 (PoE Ethernet and Wi-Fi boards) |
| On real hardware | None yet: no board, zone, siren or Zigbee coordinator connected (host tests, the build and the panel in a browser only) |

## 🚦 Project status

Each line is one part of this repository and the light says how far it is today. It is written from what the repository can show (tests, builds, runs), not from what is planned.

🟢 Done and verified · 🟡 Works, with known limits · 🟠 Partly done or not verified yet · 🔴 Not done or not working

| Area | State | What it means here |
|---|---|---|
| Software | 🟢 | The firmware builds in the ESP-IDF 5.5.5 container for both boards (Wi-Fi and Ethernet). |
| Tests | 🟢 | 486,690 checks in 14 host programs pass (`ctest`): the zones, the wired loops read by the ADC, the arming, delays, siren and PIN rules, the settings and the messages. |
| Protocols and data | 🟠 | The node's own Zigbee conversation with a coordinator, local or on the network, is tested only against a stand-in coordinator; it has never met a real coordinator or sensor. |
| Interface | 🟡 | The web panel is exercised in a real browser against a stand-in node, not on a board. |
| Hardware | 🔴 | No board built, no zone wired and no siren or Zigbee coordinator connected: nothing has run on a real installation. |
| Security | 🟡 | The PIN with lockout, the set-up code, the login limit and the signed updates are tested on the host; there has been no independent review and this is not a certified alarm system. |
| Updates | 🟠 | The hash and Ed25519 signature checks and the rollback logic pass on the host; an update over the air has never been run on a board. |
| Writing to equipment | 🟠 | The siren and the relays are driven by the alarm rules and tested against stand-ins; none has been connected to a real siren or relay. |
| Integration | 🟢 | The messages of the node are validated against the shared schemas and conformance vectors of ARMOR-COMMON. |
| Documentation | 🟢 | Eight languages, the firmware guide, the wiring notes and the changelog are up to date. |

## 🎯 Overview

* **Zones:** four kinds - *instant* (a door, a window), *entry* (the door you come in through: it starts the entry delay), *interior* (motion: only in "away") and *always* (smoke, gas, panic: armed or not). A zone may be a wired loop or a Zigbee sensor.
* **The wired loop:** a contact with an end-of-line resistor, read as a voltage, gives four states in one pin - normal, triggered, and tamper for a shorted or a cut wire. The limits are settings; a debouncer keeps a bouncing contact from raising the alarm.
* **The panel:** *away* and *stay* arming, an exit delay that does not treat the person leaving as an intruder, an entry delay, an alarm that **latches** until a correct disarm, a siren that stops by itself after a time, bypass of open zones when forced (never a sounding smoke detector), and a log of the last hundred events.
* **The PIN:** arming needs none, disarming and silencing do; wrong PINs are counted and, after a few, everything is refused for a while, even the right PIN.
* **Silence without disarming:** while the siren sounds, a correct PIN stops it at once with the system still armed - the alarm stays latched and recorded, a zone that has not sounded yet may sound it again, and a wrong PIN counts toward the lockout like a wrong disarm PIN.
* **The plan:** a gateway on the electrical node's board (a Zigbee radio, wired zones, a siren relay) that works with ARMOR-SERVER and **keeps guarding when the server is away**: a panel takes the radio over from the server through the Zigbee bridge of ARMOR-NODE-ELECTRICAL. See [the design](docs/DESIGN.md) and [the failover](docs/FAILOVER.md).
* **The node:** firmware for the ESP32-S3 (PoE Ethernet or Wi-Fi) with up to eight wired zones, the siren on a relay, the web panel of every node in eight languages, the Zigbee radio lent to Zigbee2MQTT, and the messages `armor/alarm/<node>/state|command|result`; the server may arm and disarm only when three independent switches allow it. See [the firmware](docs/NODE_FIRMWARE.md), [the messages](docs/ALARM_MESSAGES.md) and [the hardware](docs/HARDWARE.md).
* **Taking the radio over:** after the server's Zigbee2MQTT has been away for a set time the node brings the coordinator up itself (ZNP), reads the sensors of the zones that name one (IAS Zone) and commands Zigbee sirens (IAS WD), and lets go the moment the client returns. **Not yet:** a real coordinator or sensor, the hardware and any test on a real installation.
* **A restart does not disarm the house:** the mode, a running alarm and the zones left out are kept in flash and taken back at start; a cut or shorted loop is never left out; messages go through a queue (no deadlock with the broker) and the watchdog restarts the board if the task that reads the zones stops.
* **Signed over-the-air updates:** the node installs from GitHub only an image whose SHA-256 matches and that carries a valid Ed25519 signature of the maintainer (the public key is in the firmware, the private one never leaves the maintainer); a file uploaded by hand by an administrator in the panel is still accepted. The pages and messages of the panel are in eight languages.
* **Bluetooth pairing with a PIN:** the six-digit PIN is derived from the set-up code, printed on the USB console and shown to an administrator in the panel; the phone has to enter it before it may write the node's settings.
* **Siren and restarts:** the siren's relay belongs to the alarm alone (the broker, the central and the panel cannot move it, and it does not fall back to open when the broker is lost); a zone that has not sounded yet sounds the siren again after it timed out; an alarm of smoke or panic with the system disarmed, a silenced alarm and a lockout come back after a restart; a reset by hand is refused while the alarm is armed; a Zigbee sensor that never speaks counts as lost.
* **Only the house may use the panel:** the panel answers the private network by default (or the networks its owner lists, such as a VPN), refuses a list that would lock out whoever saves it, counts wrong passwords for each person as well as each address, and works behind the server's proxy so that it can be reached from outside the house without publishing the node.**A zone that cannot be read is not shown as closed:** if the converter does not start or a loop gives no reading for five rounds in a row, the zone is in *trouble* - it is not guarding, it never sounds the siren, it stops an arming (unless forced, and then it is left out and noted) and the server warns.
* **Optional encrypted link to the broker (TLS):** off by default, so the node talks plain MQTT as before. A switch and a box for the certificate of the installation's own authority (PEM, at most 2 KB) in the Broker page make it connect with `mqtts://`, trust that authority only and check that the broker's certificate was made for the address it reached; anything that is not one PEM certificate is refused. Covered by host tests; the handshake has not been run on a board yet.
* **Arming profiles:** an arm command may carry a profile (Away, Home, Night, your own) that chooses, for that arming, which zones guard, which alarm at once or after the entry delay, the exit and entry delays and whether the siren sounds (a silent alarm is still latched, recorded and told). It never touches smoke, panic or tamper zones, the PIN, the lockout, the silence, a forced arming or a zone in trouble; the profile survives a restart, and an arm without one behaves exactly as before. Tested against the old rules with a random run and against the shared contract.
* **Health report:** about once a minute the node publishes how long it has been up and why it last restarted, its free memory, its link and Wi-Fi signal, how many times it connected to the broker and the last error it logged (`armor/health/<node>`, the `node_health` contract of ARMOR-COMMON). The code that builds it is shared by every node and tested on a computer; it has not run on a board.
* **Control from the server:** Studio can restart the node cleanly, pause it for up to an hour (it resumes by itself), put its work in standby (no end, kept across restarts, the node stays connected and keeps answering), make it show itself, ask for a diagnostics snapshot and mark it as in maintenance (`armor/control/<node>/...`, the `node_control` contract of ARMOR-COMMON). Every order takes two steps with a one-time token that belongs to that operation, only the server may write them on the broker, and there is no factory reset, no erase and no deep sleep. While the node is paused or in standby its zones are not read, nothing is evaluated, armed or sounded, no state is published and the siren relay is held open: the house is NOT protected, and the health report says so. The code is shared by every node and tested on a computer; it has not run on a board.
* **Diagnostics without a cable:** after a start that was not clean (a crash, a watchdog, a sag of the supply, a failed allocation) the health report says why for ten minutes; a crash leaves its core dump to look at, download or erase from the panel; the stored settings are copied before every firmware install and recovered if an update damages them; and an administrator can ask the node, through the server, for a diagnostic package or its log with a one-time token.
* **The server's security options:** the panel applies the options the server keeps for it (`armor/alarm/<node>/policy`) and each can be fixed on the node itself: the saved armed state is tied to the zones and the PIN, the server's Zigbee zone messages can be switched off, and when signed they are checked (signature with a key typed in the panel, and a sequence number that only goes up). Tested on a computer against the values the contract and the server compute; not run on a board.
* **Optional sealing of stored secrets:** off by default (`security.seal_secrets`, or the server's `nodes.nvs_encryption`). When on, the passwords, tokens, keys, PINs and password hashes the node stores are kept sealed with ChaCha20-Poly1305 and everything else stays readable; turning it on or off rewrites them with nothing lost, even if the node stops half-way. No eFuse is burned and the flash is not encrypted: the key lives in a partition of the same chip, so it does **not** protect against someone who holds the board, and a node installed before that partition existed keeps its secrets as they were until it is flashed with the new partition table.

## 🖼️ Screenshots

Captured at 1920×1080 on a test bench, with sample data and no real installation behind them.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Overview: the state of the node at a glance - network, broker, firmware and what it reports." width="100%"><br><sub>Overview: the state of the node at a glance - network, broker, firmware and what it reports.</sub></td><td width="50%" valign="top"><img src="images/screenshots/zones.png" alt="Zones: each wired or Zigbee zone of the panel, its kind, hold time and live state." width="100%"><br><sub>Zones: each wired or Zigbee zone of the panel, its kind, hold time and live state.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/zigbee.png" alt="Zigbee radio: the serial bridge to the coordinator module, which client may use it, and the state of the bridge." width="100%"><br><sub>Zigbee radio: the serial bridge to the coordinator module, which client may use it, and the state of the bridge.</sub></td><td width="50%" valign="top"><img src="images/screenshots/inputs-outputs.png" alt="Relays and siren: the outputs of the base board and what drives each one." width="100%"><br><sub>Relays and siren: the outputs of the base board and what drives each one.</sub></td></tr>
</table>

## 🧩 Role in A.R.M.O.R.

ARMOR-NODE-ALARM is the alarm panel: the zones, the arming, the delays, the siren and the PIN are decided on the node itself, so the house is still guarded when the server is away. [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) shows it, ties Zigbee sensors to its zones and, when you allow it, arms and disarms it; the *Alarm panels* menu and the *Alarm Designer* of [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) and the [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) app drive it. Its Zigbee radio can be on the board or a coordinator on the network.

## 📂 Repository Structure

```text
ARMOR-NODE-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), alarm_supervisor (what it keeps in flash and when it speaks), arm_profile (the arming profiles), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (eight languages)
├── tests/   test_alarm (the rules), test_profiles (the arming profiles), test_node (settings, messages, runner), test_mqtt_tls (the optional encrypted link to the broker), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ Development Environment

**Requirements:** a C++17 compiler for the host tests, the ESP-IDF 5.5.5 container to build the image, an ESP32-S3 board (PoE Ethernet or Wi-Fi)

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 486,690 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

See the [design](docs/DESIGN.md), [how the panel takes over from the server](docs/FAILOVER.md) and the [safety notes](docs/SAFETY.md).

## 🔗 Related Projects

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) is a perimeter-security system made of independent repositories. Each one has its own version, its own tests and its own README; this is the family:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Message contracts, validators, conformance vectors and generated types
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Field-node firmware for ESP32-S3 with three radars and its own web panel
* **[ARMOR-NODE-SOLAR](https://github.com/JuanenRac/ARMOR-NODE-SOLAR)** - Solar inverter and battery protocols and the messages of a gateway node
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Electrical node: meters, the message of the network's readings and the rules for switching
* **ARMOR-NODE-ALARM** (this repository) - Alarm node and panel: zones, arming, delays, siren and PIN, with the server or without it
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Touch panel: the state of the system on a wall screen, arming and acknowledging, and the home of the voice assistant
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - The local network: its devices, the internet and what changes
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Central coordinator: telemetry, alarms, devices, solar readings and cameras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web console: cameras, radar, alarms, solar energy and the 2D/3D site designer
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Android operator client with a live 2D/3D radar
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - iPhone operator client
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Visual inference policy that explains its decisions and never actuates
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Camera zones, masks and detection rules for the Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Offline voice intents with a confirmation that cannot be forged
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Enclosures, electronics and the bench acceptance matrix
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Deployment, the CM5 test bench, backup and TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Offline telemetry simulator with repeatable faults
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Detects, installs and updates the ecosystem's own repositories
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architecture, security baseline and the capability matrix

## 📚 Documentation & Community

Where to read more:

* [Capability matrix: what is proven and what is not](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Project catalogue: versions and how the repositories depend on each other](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Changelog of this repository](CHANGELOG.md)
* [License (GPL-3.0-or-later)](LICENSE)
* Questions, ideas and reports: electrohobby3d@gmail.com

## 👤 AUTHOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENSE

GPL-3.0-or-later - see [LICENSE](LICENSE).
