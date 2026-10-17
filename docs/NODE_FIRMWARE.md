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
