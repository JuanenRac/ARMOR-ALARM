# Changelog

All notable changes to this project are documented here.

## [0.2.8] - Zigbee zones told by the server and a PIN for Bluetooth pairing

- The Zigbee guide lists the radio modules that work with the bridge and names the RF-star RF-BM-2652P4 (TI CC2652P7: twice the memory, Zigbee 3.0, Thread, Matter and Bluetooth 5.3) as the module for the first boards, with its supply, reset, JTAG, antenna and footprint notes; the older CC2652P modules stay supported as before.
- Bluetooth pairing now asks for a 6-digit PIN that the node prints on its USB console next to the setup code; the link is authenticated and nothing is accepted over Bluetooth without it (until now any phone in range could pair without a PIN).
- The Bluetooth note of the panel explains the PIN in all eight languages.
- While the server holds the Zigbee radio, the panel now takes the state of its Zigbee zones from the server (armor/alarm/<node>/zigbee), so arming the panel guards those doors, windows and motion sensors instead of leaving them at their last state; when the node holds the radio itself it hears the sensors directly and ignores the server.
- The message is strict (zone, state normal, triggered or tamper, battery_low), is information only and cannot arm, disarm or sound anything; a message for a wired or unknown zone changes nothing.
- The broker note of the panel named the wrong identity and topics (it was copied from the electrical node): it now says alarm-node and armor/alarm/<node>/state and /result, in all eight languages.

## [0.2.7] - Portuguese in the panel and a warning for the access point and the station at once

- The panel speaks Portuguese, the eighth language, in every page, hint and message of the settings.
- The overview warns when the node's own Wi-Fi network and its connection to your router are on at the same time (they share one radio and one channel), and the Wi-Fi help explains what that costs and when to switch the access point off.

## [0.2.6] - Slot copy, background update check and supervision of silent Zigbee sensors

- A firmware slot can be copied to the other one from the panel, and the update of a node waits for the image to be confirmed only after the node has an address or its own access point is reachable, rolling back after five minutes otherwise.
- The station connection that was paused to leave the access point free retries after ten minutes at most instead of waiting forever.
- The firmware check against GitHub runs in the background and the panel asks for its result, so a slow search no longer freezes the panel.
- Saving the settings is serialised, so two browsers saving at once cannot mix their changes.
- Logging out needs the page's own token, which stops another page from logging the panel out.
- A Zigbee sensor that has been silent for longer than the configured number of minutes counts as lost and, while the system is armed, as a tamper on its zone; 0 turns the supervision off.

## [0.2.5] - Copy-log icon and copy of a firmware slot to the other

- **Copy one firmware slot to the other:** next to the switch between ota_0 and ota_1, the Firmware card of the panel has a button that copies the firmware that runs over the other slot (task of its own, with a percentage, the image checked at the end, no restart, no change of the boot slot); `POST /api/v1/ota/copy` starts it and `GET /api/v1/ota/copy` tells how far it is. The slot sizes shown by the status are measured again after a copy or an update.
- **Node panel:** the log of the panel has a small copy icon (two sheets) that puts the whole log on the clipboard and says "Copied" for a moment; the same panel code is shared by all the nodes.

## [0.2.4] - Lockout that grows and a first reading that holds

- Wrong PINs lock the panel for longer each time: the base time, then five times and then fifteen times as long, until a right PIN is entered.
- The first reading of a zone after a start has to hold like any other, so a converter still settling at power-up can no longer raise a false tamper; tests for both.
- Looking for a new version on GitHub no longer fails with connect_failed when the node was short of internal memory (a browser on the HTTPS panel at the same moment): the TLS buffers now live in the external memory, and the search and the download try three times.

## [0.2.3] - Hardening of the web server, the broker link and the sessions

- The web server gives up a request that announces a body and sends nothing (or an upload that stalls) after fifteen seconds, instead of keeping its only thread for ever.
- The restart after an installation from GitHub no longer depends on a browser asking how far it is: a timer of the node watches the installation and restarts it when it is done.
- A "remember me" session is no longer taken for free after thirty minutes when the table fills up, and a session token that starts again replaces the old one instead of living twice.
- On a saved broker that is not the first one, the node tries the first one again every half hour instead of forgetting it until the next restart.

## [0.2.2] - A restart no longer disarms the house; no deadlock; sabotage cannot be left out

- **The armed state is kept in flash** (mode, an alarm that was running, the zones left out) every time it changes and taken back at start: a power cut, a reset by the watchdog, an update or the automatic restart no longer leave the central disarmed. An exit delay that was running comes back armed; an entry delay that was running (nobody had disarmed) comes back as an alarm. A record that is not exactly a saved state arms nothing.
- **A message is never published while a lock of the node is held:** `mqtt_link::publish()` puts the message in a short queue (48, the oldest dropped first) and a task of its own hands it to the client. Before, a task that held a lock of its module and published could wait for the client's own lock while the client, running the handler of an incoming command, waited for that module lock: both for ever, with nothing to restart the board.
- **The task watchdog restarts the board:** it panics after 10 s (it only printed before) and the tasks that guard the outputs now report to it. The siren relay's own task reports as well.
- **A cut or shorted loop (tamper) is never left out:** forcing an arming is refused while a loop is in tamper, and a zone that was left out still sounds when its wire is cut.
- An alarm with the central disarmed (a smoke, gas or panic zone, or a tamper) is now accepted by the server and the contract (ARMOR-COMMON 0.4.6). Host tests: restart and sabotage scenarios added.

## [0.2.1] - Plain contacts, buttons and a central that needs no broker

- **Plain contacts as zones.** A zone can be a switch to ground on any free pin instead of a loop with an end-of-line resistor (`input: digital`): the node pulls the pin up, a closed contact reads low, and the zone says triggered when a contact that is closed while all is well opens (`contact: nc`) or when one that is open while all is well closes (`contact: no`, a panic button or the dry output of a sensor). With `role: tamper` the switch of a box or a detector's lid says tamper, which raises the alarm in every state of the system. A digital zone needs no loop limits and is debounced like the others. The panel's *Zones* page asks for the kind of input, the contact and the role; the pins offered are every pin the board allows, not only the analog ones.
- **Buttons that arm.** Up to four push buttons on free pins arm the panel (away or at home) with one press, once per press, debounced; arming needs no PIN, a press while armed is refused, and no button disarms (only the PIN does).
- **A direct, authenticated HTTP API for a central** that has no broker to rely on (off by default; `remote.http` and a token the panel can generate and never shows again): `GET /api/v1/remote/state` reads the node (its relays and the panel's state with each zone) and `POST /api/v1/remote/relay` {"relay":"name","action":"on|off|toggle"} moves one relay; on this node also `POST /api/v1/remote/alarm` arms (`{"action":"arm","mode":"away|stay"}`) or disarms (`{"action":"disarm","pin":"..."}`) with the panel's own rules (arming needs no PIN, disarming does, and a few wrong PINs lock the panel). The token travels as `Authorization: Bearer <token>`, is compared without leaking where it differs, must be at least 20 characters, and a wrong, missing or unset token answers 401 without saying which. Tested on a computer (`core/central_link.hpp`); the routes have never answered on a board.
- **Waits that grow between tries.** A Wi-Fi network that does not answer is tried again after 1 s, then 2, 4, 8... up to 30 s (a little scatter on every wait, so that many nodes that lost the same router do not all knock together), and the broker after 1 s growing to 60 s; a try that works starts the schedule over. Before, every retry was a fixed 3 s (Wi-Fi) or the client's fixed 10 s (broker). The schedule is `core/backoff.hpp`, tested on a computer.
- **Improv Wi-Fi over the USB cable.** A web page that speaks the Improv standard (ESP Web Tools) can give the node its Wi-Fi network through the same USB socket the log comes out of: the node answers its state, identifies itself, describes the device and takes the SSID and password (checked and stored through the same settings document the panel and the Bluetooth set-up use), then restarts to join. Whatever else is on the line is ignored and a packet is only acted upon whole and with a right checksum. The packets are `core/improv.hpp` (tested on a computer); the USB side has never run on a board.
- **The stack each task has left**, in the status (`tasks`: the least room ever left, smallest first, with a `low` flag under 512 bytes) and as a warning in the log, once per task, before a near overflow becomes a crash. `core/task_health.hpp` (tested on a computer) and `main/task_health.cpp` (FreeRTOS' trace facility, switched on in the defaults).
- **Tests:** the contacts, the buttons and the zones that use them (settings, the document, the runner at work), the schedule of retries, the Improv packets, the token and the stack report; the panel in a real browser (35 checks on each board).

## [0.2.0] - The node takes the Zigbee radio when the server is away

- **The node speaks Zigbee itself while the radio is its own.** When no client (the server's Zigbee2MQTT) has been connected to the bridge for the time of the new setting *Take the radio after* (`zigbee_local.after_s`, from 10 s; 0: never), the node holds the radio: it brings the coordinator up (`SYS_PING`, `AF_REGISTER` on endpoint 1, `ZDO_STARTUP_FROM_APP`, waiting for the network to say it is up), asks for the short address of every sensor and siren it was given by their long address, and lets go the moment a client connects again (`core/radio_owner.hpp`).
- **Zigbee zones:** a zone may name a sensor by its long (IEEE) address instead of a wired pin. A sensor's IAS Zone *Zone Status Change Notification* becomes the state of its zone (alarm: triggered; tamper: tamper; closed again: normal; a low battery is kept), through the same panel rules as a loop, so an entry zone starts the entry delay from a door contact as from a wire. A Zigbee zone has no reading while the server holds the radio.
- **Zigbee sirens:** up to four (long addresses); the node tells them to sound (`IAS WD Start Warning`, burglar, high, with strobe) and to stop with the panel's siren, only while the radio is its own. The siren relay of the base board is unchanged.
- **The code** (all of it on a computer, no radio): `core/znp.hpp` (ZNP frames with a parser that resynchronises after a lost byte or a wrong checksum, and the few commands used), `core/zigbee_ias.hpp` (AF_INCOMING_MSG, the ZCL frame, IAS Zone status change and enroll, IAS WD start warning), `core/local_zigbee.hpp` (the conversation: steps, retries, address lookups, updates, sirens) and `core/radio_owner.hpp`. The bridge's own copy for this project takes a hook for the host (`main/zigbee_bridge.*` are no longer shared with the electrical node).
- **The panel:** the zones page has the sensor's address; the Zigbee radio page has the time and the sirens; the radio's state can be "held by the node"; seven languages. 29 checks of the page in a real browser.
- **Tests:** `tests/test_zigbee.cpp`, 86 checks: the frames (the SYS_PING bytes, one byte at a time, noise, a bad checksum, a lost byte), the clusters (a door, tamper, closing, what is not a notification), the whole start-up against a stand-in coordinator that answers as the protocol says, the sensors' states, an announce moving a short address, the sirens, an enroll, a coordinator that does not answer, who holds the radio, and the settings and the runner with a Zigbee zone.
- **Not done:** nothing has talked to a real coordinator or sensor: the layouts are the ones Z-Stack and the Zigbee Cluster Library document, and the first real start-up will probably need adjustments (for instance the endpoint registration, or a coordinator whose network was never formed). The HTTP API between a separate panel and the nodes for when the broker is away too is not written.

## [0.1.0] - The firmware of the alarm node

- **The node firmware** (ESP32-S3, ESP-IDF 5.5; `s3-eth` for the Waveshare ESP32-S3-ETH on PoE and `s3-wifi` for an ESP32-S3-WROOM-1): up to eight wired zones read on ADC1 pins, the panel of 0.0.1 running on them, the siren on a relay of the base board, and a task that publishes the state and listens for commands. A zone whose pin cannot be read says nothing at all, so a fault of the node is never taken for a tamper.
- **The settings** (`core/alarm_config.hpp`) on top of what every node shares (`core/node_settings.hpp`) and the base board's radio and relays (`core/io_config.hpp`): zones, the loop's limits, delays, siren, PIN (stored, never shown) and the server's switch. Settings are checked before they are saved and a repeated zone identifier or a PIN that is not four to twelve digits is refused.
- **The messages** `armor/alarm/<node>/state|command|result` (`core/alarm_message.hpp`, [docs/ALARM_MESSAGES.md](docs/ALARM_MESSAGES.md); the schemas and the conformance vectors are in ARMOR-COMMON): the state carries the phase, the mode, the siren, the zones, the open zones and the last ten events; a command is read strictly and gets no answer when it is anything but exactly the contract. The server may arm and disarm only when the node's setting, the broker's access list and the server's own switch all allow it, and a disarm from the server carries no PIN.
- **The web panel** in seven languages: the alarm (arm, disarm with the PIN, the zones, the events), the zones and the loop's limits, the delays and PIN, the Zigbee radio, the relays and siren, and the pages every node has (network, Wi-Fi, broker, users, update over the air with rollback and from GitHub, log, HTTPS, Bluetooth set-up).
- **The base board:** the Zigbee radio bridge, the relay outputs (they start open) and the update from GitHub come from ARMOR-COMMON's firmware base, the same files as the electrical node. Arming is never refused because of a lockout; only disarming is.
- **Tests:** 492,368 checks in all on a computer (the rules, the settings, the messages, the runner and the update from GitHub), and the panel in a real browser against a stand-in node (25 checks on each board). The firmware builds for both boards. The messages the serialiser prints (10 states, 6 commands, 6 results) are checked against ARMOR-COMMON's published contract (`tests/emit_samples.cpp`, `tests/check_samples.py`).
- **Documents:** the firmware, the messages, the hardware proposal (an end-of-line loop and the siren), the base board, the Zigbee radio and the Bluetooth set-up; the design, the failover and the safety notes now describe the firmware.
- **Not done:** nothing has run on a board or on a real installation, no zone has been wired and no siren connected; the Zigbee stack the panel needs to take the radio over from the server is not written. This is not a certified alarm system.

## [0.0.1] - The core of the alarm: zones, the loop and the panel's rules

- **Zones** (`core/alarm_zone.hpp`): four kinds - instant, entry, interior and always - and three states (normal, triggered, tamper). A zone may be a wired loop or a Zigbee sensor.
- **The wired loop:** a contact with an end-of-line resistor, read as a voltage, gives normal, triggered, or tamper for a shorted or a cut wire; the three limits are settings and are checked (ascending, within the converter's range). A debouncer makes a reading hold before it counts, so a bouncing contact does not raise the alarm.
- **The panel** (`core/alarm_panel.hpp`): arming in *away* and *stay*, an exit delay in which only an always-zone or a tamper alarms, an entry delay started by an entry zone, an alarm that latches until a correct disarm, a siren that stops by itself after a time, bypass of open zones when arming is forced (never a sounding always-zone), a zone that alarms only when it *changes* to triggered (a smoke detector still sounding after a disarm does not restart the alarm), and a log of the last hundred events.
- **The PIN:** arming needs none; disarming and silencing do. Wrong PINs are counted and after a few everything is refused for a while, even the right PIN; the comparison does not depend on where the PINs differ, and a panel with no PIN set never disarms.
- **Tests:** scripted scenarios for every rule and a long random run (60 rounds of 3,000 steps) that checks what must always hold - a siren means an alarm, an alarm ends only with a correct disarm, a wrong PIN never disarms, a quiet history keeps a disarmed panel quiet, the entry delay never overruns - 492,806 checks in all; a broken PIN check and a siren that never times out are both caught.
- **Documents:** the design, how the panel takes over from the server (`docs/FAILOVER.md`) and the safety notes. There is no firmware, no Zigbee stack and no message with the server yet; nothing has run on a board.
