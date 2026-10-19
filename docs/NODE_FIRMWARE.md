# The firmware of the alarm node

The firmware of an **ESP32-S3** node on the electrical node's base board ([BASE_BOARD.md](BASE_BOARD.md), [HARDWARE.md](HARDWARE.md)): the Waveshare ESP32-S3-ETH on its PoE cable (`s3-eth`) or an ESP32-S3-WROOM-1 N16R8 on Wi-Fi (`s3-wifi`). It reads **up to eight wired zones** (a loop with an end-of-line resistor on an analog pin), runs the **alarm panel** (core/alarm_panel.hpp), drives the **siren** through a relay of the base board, tells the server what it sees (`armor/alarm/<node>/state`, [ALARM_MESSAGES.md](ALARM_MESSAGES.md)) and lends a **Zigbee radio** to Zigbee2MQTT ([ZIGBEE.md](ZIGBEE.md)).

It has the web panel of every other node (set-up, login, users, Wi-Fi, broker, update over the air with rollback and from GitHub, log, HTTPS, Bluetooth set-up) - the files they share come from ARMOR-COMMON's firmware base - and its own pages: **Panel** (the state of the alarm, arm, disarm with the PIN, the zones and the last events), **Zones** (the loops and their limits), **Delays and PIN**, **Zigbee radio** and **Relays and siren**, in the eight languages.

## What is where

- `core/alarm_zone.hpp`, `core/alarm_panel.hpp`: the zones, the loop and the panel's rules (see [DESIGN.md](DESIGN.md)).
- `core/alarm_config.hpp`: the settings (zones, the loop's limits, the panel, the siren, the server's commands) on top of `core/node_settings.hpp` (what every node has) and `core/io_config.hpp` (the base board's radio and relays).
- `core/alarm_message.hpp`: the state, the command and the answer, checked against ARMOR-COMMON's contract by `tests/emit_samples.cpp` and `tests/check_samples.py`.
- `core/alarm_runner.hpp`: what the node does on every turn - the readings go in, the panel's state and the siren come out.
- `main/alarm_manager.cpp`: the task that reads the ADC (ADC1 only), feeds the runner, drives the siren, publishes and listens for commands.
- `main/web_server.cpp`, `main/api_shared.cpp`, `panel/`: the panel's server and pages.
- `main/relay_outputs.cpp`: the relays, the same file as the electrical node's (the siren's relay is reserved to the alarm). `main/zigbee_bridge.cpp` is this project's own: it also lends the radio to the node itself and talks to a coordinator on the network.

## What it does and does not do

- **Not a certified alarm system.** See [SAFETY.md](SAFETY.md).
- A zone whose pin cannot be read says nothing at all: a fault of the node is never taken for a tamper, so it cannot sound the siren by itself.
- Arming needs no PIN; disarming and silencing the siren do (a wrong PIN counts toward the lockout either way). The server may arm, disarm and silence only when three independent settings allow it.
- **Nothing here has run on a board**, no zone has been wired and no siren connected. The host tests (the zones, the panel, the settings, the messages and the runner) and the panel in a real browser against a stand-in node (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`) are what has been done; the firmware builds for both boards.
- **Taking the radio over** while the server is away is written (core/local_zigbee.hpp, [FAILOVER.md](FAILOVER.md)) and has never run against a real coordinator or sensor; a separate panel reaching the nodes over HTTP when the broker is away too is not written.

## Health report

About once a minute the node publishes a health report to `armor/health/<node>` (the `node_health` message of ARMOR-COMMON, see its `docs/CONTRACTS.md`): the firmware version and board, how long the node has been up and why it last restarted (`power_on`, `software`, `panic`, a watchdog, `brownout`...), the free heap now and the least it has had, the link it uses (Wi-Fi with its signal in dBm, or the Ethernet cable) and whether it is up, how many times it has connected to the broker since it started, how many tasks are short of stack and the last error line it logged. It is built by `main/node_health.cpp` and `core/node_health.hpp`, which are shared by every node (`ARMOR-COMMON/tools/sync_firmware_base.py` keeps the copies identical); the choices it makes - the words for a reset reason, which link the node is on, the last error of the log - are tested on a computer in `tests/test_base.cpp`. The report is information only: the node decides nothing from it and nothing is sent back. The broker must let the node write `armor/health/<node>` (`scripts/mqtt_identity.sh add` of ARMOR-DEVOPS does it for a new node, `health-report` for one installed earlier); the server judges the reports and raises its warnings. It has not run on a board.

## Memory

The board has two kinds of memory: some 300 KB of internal memory, which the radio, the drivers and every task stack share, and 8 MB of external RAM (PSRAM) that nothing may touch while the flash is being written. The firmware keeps the internal memory for what needs it and puts the rest outside:

* **TLS** (the HTTPS panel, the update from GitHub, an encrypted broker link) takes its memory from the external RAM and, on a board where it is missing or full, from the internal memory (`CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC` with the allocator of `main/psram_mem.cpp`; the stock "external" mode would fail every request on a board without PSRAM).
* **Requests above 4096 bytes** (`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`) go to the external RAM first: the texts and tables of the panel, the JSON documents. 32 KB of the internal memory stay reserved for what must be there.
* **Task stacks**: the tasks that only read counters and queue messages, and never touch the flash, have their stack in the external RAM (`memory::start_task`): the check for a newer release (12 KB), the broker link's sender and watchdog, the health report. The tasks of the radio, the network, the settings, the update itself and the equipment stay on internal stacks.
* **Seen in the log**: every minute the line `memory: internal N KB free (least N KB, biggest block N KB), PSRAM N KB free of N KB, N task stacks (N KB) in PSRAM` (a warning when the internal memory is under 40 KB), and in the status of the panel and in the health report `heap_internal_free`, `heap_internal_min`, `heap_internal_largest` and `psram_free`. The figure `heap_free` adds the external RAM, so it never looks short; judge the memory by the internal figures.

None of it has been measured on a board yet: compare the line of the log before and after the update on the same node, with the same radars and the panel open, and expect more internal memory free than before.

## Self-protection

A node that runs for months has to notice when it is in trouble and put it right by itself, without turning into a restart loop. The rules are pure functions in `core/node_guard.hpp` and `core/ble_window.hpp` (tested on a computer in `tests/test_base.cpp`); `main/node_guard.cpp` reads the real values and acts. All of it is shared by the five nodes (`ARMOR-COMMON/firmware_base`). The thresholds are starting values, to be compared with the readings of a real board.

* **The tasks the node needs exist before Bluetooth starts.** The broker link's tasks (and, in the radar node, the heartbeat and the telemetry) are made at start-up, not when the network comes up, and Bluetooth is the last thing `app_main` starts. Before, the Bluetooth stack took the internal memory first and the telemetry task was never created: the node ran for days publishing a few dozen messages. The tasks that only build and queue messages and never touch the flash have their stack in the external RAM.
* **Bluetooth memory.** The host stack of NimBLE is allocated in the external RAM (`CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL`) and the controller is given the three activities it uses (`CONFIG_BT_CTRL_BLE_MAX_ACT`). Bluetooth is not started at all when less than 40 KB of internal memory is free, and the log says the internal memory before and after it starts.
* **The set-up window closes.** In mode `setup` Bluetooth switches itself off after `CONFIG_ARMOR_BLE_SETUP_MINUTES` minutes (10 by default, 1 to 120) with nobody connected, or two minutes after a node that already has users gets an address; the stack is stopped and its memory freed, and it stays off until the node is restarted. Mode `always` is the owner's own choice and never closes.
* **The guard.** Every two seconds it looks at the tasks the node registered as needed and at the internal memory. If a task is missing, or the internal memory stays under 16 KB (back to normal over 24 KB), for a minute, it logs it (the line is also the last error of the health report, which is sent at once) and restarts the node once, cleanly. At most three such restarts in an hour of running time: after the third the node stays up and repeats the alarm every ten minutes. After a restart the guard asked for, Bluetooth stays off for that start.
* **The gate at the door of the panel.** While the internal memory is under 28 KB, or its biggest block under 12 KB, a new connection to the panel is refused before anything is allocated for it; it opens again over 40 KB and 16 KB. Open connections, the broker link and the node's own work are not touched; the log says each change with the memory at that moment.
* **A failed allocation** is not left to abort: the incident is written in memory that survives a restart and the node restarts at once; the next start logs `oom heap=<free>/<largest> after <s> s` (the last error of the health report). During the write of a firmware image, or a second time within a minute of running time, it aborts as before so that a loop stays visible.
* **The dead-man switch.** With a broker set up and connected, ten minutes without handing any message to it restart the node (same limit of three an hour); after that it stays up, degraded, and says so.
* **Last will and birth.** The broker link's last will on `armor/health/<node>` is retained, and the node says `{"node_id": "<node>", "online": true}` (retained) each time it connects; the radar node does the same on `armor/node/<node>/health`. The last message of the topic is always the state of the node, also for a server that starts later.
* **Building.** A cached `sdkconfig` keeps the old value of a line added to `sdkconfig.defaults`: delete `.compile/<profile>` (or `build/<profile>`) before building, and run `python tools/check_sdkconfig.py` afterwards; it lists every line of `sdkconfig.defaults` that the build did not take.

None of it has run on a board. To check on one: the log at start (`starting Bluetooth with N KB`, `Bluetooth is up`, `Bluetooth is off`), the minute line of the memory, and a forced shortage of memory to see the gate and the guard act.
