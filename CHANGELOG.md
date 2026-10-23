# Changelog

All notable changes to this project are documented here.

## [Unreleased]

- The repository has a security policy, a code of conduct, a contributing guide and a support file.
- The banner follows the animated style of the ecosystem banners, with the name, the kind and the technologies of the project.
## [0.4.4] - The server's security options applied by the panel, the zone feed checked, wrong passwords per minute as a setting and interlocked relays

- **Fixed: a node with stored settings no longer restarts in a loop at power-up.** The start-up code read the stored settings and the copy made before the last update on the stack of the main task (about 3 KB each, plus the reader's own copy); with settings stored the 16 KB stack overflowed and the node restarted again and again before it could start the network. They now live on the heap.
- **The panel applies the options the server keeps for it** (`armor/alarm/<node>/policy`): it keeps the last it heard in flash, so they hold from its first moment after a restart, and until the server speaks it does what it always did. Each of the three can also be fixed on the node (*Delays and PIN*, *Security options of the server*): follow the server, on, or off.
- **The saved armed state is tied to the zones and the PIN:** with the option on, a restart does not give the armed state back to a panel whose zones or PIN changed (it starts disarmed and says why in its log); a state saved before the option comes back as before.
- **The Zigbee zone messages of the server are checked:** a signed message needs the right signature (HMAC-SHA256 with the key of the panel, which an administrator types in the panel and is never shown again) and a sequence number above the last; an unsigned message is refused only when a signature is required and the panel holds the key, so an update does not shut the feed of a panel that has no key yet; the feed can be switched off. Verified against the value the contract and the server compute.
- **A relay can name another that must never be closed at the same time;** the siren, which the alarm forces, is not held back.
- **The limit on wrong passwords per minute is a setting of the node:** `system.login_limit_per_minute` (30 as always, 0 for none, at most 120) with its field and its explanation in the panel in the eight languages; the limit of each address and of each person is not touched by it.
- **The certificate of the broker can be loaded from a file** on the Broker page (it is checked for the BEGIN and END lines, a private key is refused, and it must fit the 2048 characters the node keeps); nothing is sent to the node until the page is saved.
- **New checks in the host tests:** the Bluetooth session stays under its lock and a request or an answer never crosses to another connection (`check_ble_session.py`); the routes every node serves, the routes the panel calls and the stand-in node the panel is tried against agree (`check_routes.py`); the guard on the paths of a save also keeps the tolerant read of the stored settings, the serial set-up that makes the Wi-Fi the way in, and the answer to a release without an image for the board.
- **The universal image holds no fleet secret unless asked for:** `tools/build_node.sh generic` adds `secrets/generic.conf` only with `ARMOR_WITH_FLEET_SECRET=1`, names that image `*-local` and says it is never to be published, and a public build is refused when the configuration it ended with holds a secret or a set-up code; the option's help says the same.
- **What the alarm task decides around the panel is now tested on a computer:** what comes back from flash at start (including not giving the armed state back to a panel whose zones or PIN changed), what is written to flash and when (the state at once, the events at most every half minute unless something that matters happened) and when the state is said again live in `core/alarm_supervisor.hpp`; `main/alarm_manager.cpp` only reads the pins, does the writes it is asked for and talks to the broker. New host test `tests/test_supervisor.cpp` (118 checks) also covers arming and disarming, the PIN and its growing lockout, the server's command that needs no PIN, the exit and entry delays, the siren's timer and the options the server keeps for the panel.
- **Fixed: an arming profile left in flash that nobody uses is erased** at the next turn, so a later arming without a profile cannot bring it back after a restart.

## [0.4.3] - Node diagnostics

- **ESP-IDF 5.5.5 is the one version:** `tools/build_node.sh` uses the 5.5.5 container, and the firmware guide, the requirements of the READMEs and the manifest name it (it said 5.4 in places).
- **How the last run ended:** after a start that was not clean (a crash, a watchdog, a sag of the supply, a failed allocation, a restart by the node's own guard) the health report carries the cause for the first ten minutes, so nobody has to be at the board with a cable.
- **The memory gate and the core dump in the health report:** whether new panel connections are being refused now and whether a core dump is waiting.
- **Crash report in the panel:** a crash writes its core dump (ELF, with a checksum, up to 32 tasks) to the `coredump` partition (128 KB in the table of the repository); the Update page has a *Crash report (core dump)* card for administrators to look at it, download it or erase it. A node flashed before with the older 64 KB table keeps it and may not fit a long dump.
- **Settings that survive an update:** right before a firmware is installed the node keeps a copy of the stored settings next to them, with the version that wrote it and a check number, and reads both with the tolerant reader at every start.
- **Diagnostic package and log on request:** an administrator can ask the node, through the server, for its `diagnostics` with `detail` `package` or `log` (the same two steps and one-time token as the other orders); the answer carries the memory as it is now and the tasks with the least stack they have had free.

## [0.4.2] - The node protects its memory, and the server can control it

- The checksum and the image are read again from GitHub: the HTTP request buffer is 4 KB, because the request that follows GitHub's redirect carries an address of about a thousand characters.
- **The node protects itself when its internal memory runs short:** the tasks it cannot do without (the broker link, the health report and the node's own work) are made at start-up and Bluetooth is started last; a guard looks for them and for the internal memory and, after a minute of trouble, says it (log and an immediate health report) and restarts the node once, cleanly - never more than three times an hour, then it stays up and repeats the alarm.
- **Bluetooth uses less internal memory and closes by itself:** the Bluetooth host stack is allocated in the external RAM and the controller is limited to the three activities it uses; in mode `setup` it switches off after 10 minutes (`CONFIG_ARMOR_BLE_SETUP_MINUTES`) with nobody connected, or two minutes after a node that has users gets an address, and frees its memory; it is not started when less than 40 KB of internal memory is free nor after a restart the node asked for itself.
- **New panel connections are refused while the internal memory is short** (under 28 KB free or a 12 KB biggest block, open again over 40 KB and 16 KB), before anything is allocated for them; the broker link and the node's work are not touched.
- **A failed allocation restarts the node cleanly once** and the next start reports `oom heap=<free>/<largest>`; during a firmware write, or twice within a minute, it aborts as before. A node that hands nothing to a connected broker for ten minutes restarts through the same limit of three an hour, then stays up, degraded.
- **Last will and birth, retained:** the node says `online` true on `armor/health/<node>` each time it connects and the broker keeps its last will (`online` false) there, so a server that starts later knows the state of every node (the radar node does the same on its presence topic).
- **Memory log and the figures of the health report:** the internal memory, the external RAM and the biggest block (see the Memory section of the node's firmware guide), with the stack of the telemetry task kept at 6144 bytes.
- **Smaller fixes:** the MQTT client of the radar node no longer restarts the node when it cannot be created (it tries again), the report of a sensor's firmware and mode is written under the same lock it is read with, the Improv page says `provisioned` only after the Wi-Fi network was joined, and `tools/check_sdkconfig.py` compares a build with `sdkconfig.defaults`.
- **The server can control the node:** restart, pause, resume, standby (kept across restarts, the node stays connected), identify, diagnostics and maintenance on and off, on `armor/control/<node>/command`, with a one-time token bound to the operation; there is no factory reset, no erase and no deep sleep. While the node is paused or in standby its zones are not read, nothing is evaluated or armed, the siren relay is held open and the health report says the house is not protected. Rules and host tests are shared with the other nodes; nothing of it has run on a board.
- **Planned maintenance:** before the node goes quiet on purpose (a restart, a pause, a standby, a firmware install or copy, a panel upload) it publishes a retained `armor/control/<node>/maintenance` message (`node_maintenance`: active, why and for how long) so that the server holds back the node-offline alert for a bounded time, and clears it (retained too) when it is back; a node that boots into standby keeps saying so until it is resumed, and a window that is never closed ends by itself. The pure parts are in `control_gate.hpp` with host tests; nothing of this has run on a board.
- **The guard and the server's orders work together:** a node the server paused or put in standby is not restarted by the guard for a task it stopped on purpose (nor one with a restart order on its way; the memory rules stay in force), a restart that was ordered leaves a mark so the next start tells it from a restart of the guard and does not count it against the three an hour, the standby kept in flash survives a restart of the guard, and the planned-maintenance signal goes out before an ordered restart, an install and an upload.
- **Fixed:** the log line that closes the Bluetooth set-up window passed a text chosen at run time as the format of the message, which the logging macro does not accept at build time.

## [0.4.1] - Update from GitHub through long redirects, a richer help and the green frames of the Summary

- **Update from GitHub works again when GitHub answers with a long redirect:** the checksum file is fetched through a redirect whose headers add up to more than 5 KB, and the 4 KB header buffer made the HTTP client give up. The buffer is now 20 KB, which the memory settings place in the external RAM (above 16 KB), so it costs no internal memory; a board with no external RAM falls back to 6 KB. The log says how much internal memory was free before and after the client was opened.
- **The real reason is told:** when the checksum or the signature of a release cannot be read, the panel says it was the connection, the size or the HTTP status (new texts in the eight languages), and "the release carries no checksum" is only said for a release that really has none.
- **The Summary draws every value inside a green frame,** like the green label of a port that reports, in every node panel (one shared style rule and one helper).
- **The help of the panel is brought up to date in the eight languages:** the limit of 30 failed sign-ins per minute in total, the tolerant reading of the saved settings after an update, the update from GitHub and the notice that a release has no image for the board, Bluetooth set-up (the derived PIN and the answer only to the phone that asked), the web modes and the certificate fingerprint, and the clock and its time server.
- **The node announces that it is gone:** the connection to the broker carries a last will that tells the server the node is offline, and the server never counts it as a report.
- **No more aborts on a failed initialisation:** the network, the stored settings and the Ethernet start-up degrade with a log line instead of rebooting in a loop.
- **Fewer data races:** the cached lengths of the firmware slots, the timer that restarts the node and the flag that the restart after an update was scheduled are guarded.
- **Tests:** the header buffer against a simulated redirect of 5 KB, the reasons for a checksum or signature that cannot be read (and that "no checksum" is not one of them), the green frames in the Summary, and the words every help text must contain in the eight languages.

## [0.4.0] - Tolerant settings, global login limit, Bluetooth race, missing board image

- Settings kept in flash are read as far as they can be after an update: unknown members are ignored and a value that cannot be accepted falls back alone, so the node never starts over because of one field.
- The panel login limits the wrong passwords of all sources together to thirty per minute.
- Bluetooth answers go only to the phone that asked; waiting requests are discarded on connect and disconnect.
- A newer release without an image for this board is reported in the panel (8 languages) instead of passing for up to date.

## [0.3.9] - Shared node base update

- Shared base update: a wider login table, slot-sized download limit, stricter JSON text and a set-up code kept in capitals.

## [0.3.8] - Stored passwords follow their host, saved events and lockout step

- A stored password is tied to its host: changing the address of the broker (or of a backup broker slot) without typing a password erases the stored one instead of sending it to the new host, and the panel says under the field that the password has to be typed again. The same host (another port, other letter case) keeps it.
- The siren-silenced event is kept in flash and read back after a restart (the list of event kinds stopped one short).
- A restart in the middle of the third PIN lockout (fifteen times as long) brings back the third lockout, not the second.
- The exit and entry delays of an arming profile are range-checked before they are converted to whole numbers (infinite and enormous values are refused).
- The access point bandwidth accepts only 20 or 40 MHz (a value in between was accepted and run as 20); an address in the list of networks that may use the panel is refused when a part has a leading zero, as the address parser of the network settings already did.

## [0.3.7] - Sign-in form for password managers

- The sign-in page is a real form with named fields (`username`, `current-password`), so the browser's or system's password manager offers to save and fill the credentials; the set-up page names its fields the same way for the new password.
- A new tick, "Remember my user name on this device", keeps only the user name in the browser (off by default, forgotten as soon as it is unticked, and the panel works with storage blocked); the password is never stored by the panel.
- A short note under the form, in the eight languages, says that the password is saved only by the browser's own password manager. The existing "keep me signed in" tick is unchanged.

## [0.3.6] - Arming profiles and the health report

- An arm command may carry an arming profile (Away, Home, Night or the owner's own) that chooses for that arming which zones guard, which alarm at once or after the entry delay, the exit and entry delays and whether the siren sounds (a silent alarm is still latched, recorded and told). Smoke, panic and tamper, the PIN, the lockout, the silence, a forced arming and zones in trouble are untouched; an arm without a profile behaves exactly as before.
- The profile in use is in the state message (profile, with arm_profiles: true saying the node understands them) and is kept in flash so that a restart arms the house the same way.
- core/arm_profile.hpp reads a profile strictly as the contract has it; tests/test_profiles.cpp covers the parser, the zone resolution, the siren rules, a restart and the command, and runs the same random scenario with and without a neutral profile to prove nothing changed. emit_samples and check_samples put a profile command and its state through the shared contract.
- Every node publishes a health report about once a minute to armor/health/<node> (the node_health contract): uptime, why it last restarted, free and least free heap, the link and Wi-Fi signal, how many times it connected to the broker, tasks short of stack and the last error it logged.
- The report is built by core/node_health.hpp and main/node_health.cpp, shared by the five nodes through ARMOR-COMMON's firmware base; its decisions are covered by the host tests of tests/test_base.cpp. It has not run on a board.
- The broker link counts its connections for the report.

## [0.3.5] - An optional encrypted link to the broker (TLS)

- New settings mqtt.tls (off by default) and mqtt.ca_pem (one certificate in PEM, at most 2 KB; anything else is refused). With TLS on, the node connects with mqtts://, trusts only that authority and checks that the broker's certificate was made for the address it reached; it waits for its clock first (up to two minutes), because certificates have dates. With the switch off nothing changes: the node talks plain MQTT as before.
- The Broker page of the panel has a switch and a box for the certificate of the installation's own authority, in the eight languages, with the new error texts. The saved backup brokers must be mqtts:// too while the switch is on.
- Host tests of the settings: defaults off, the JSON round trip, line ends from a browser, and the refusal of a text that is not PEM, a chain, a text that is too long and a plain address with TLS on. The handshake has not been run on a board.

## [0.3.4] - Silence the siren with the PIN, without disarming

- A correct PIN now silences a ringing siren at once while the system stays armed: the alarm stays latched and recorded, a zone that has not sounded yet may sound the siren again (the re-sound rules are unchanged), and a wrong PIN counts toward the lockout exactly like a wrong disarm PIN.
- The panel page has a Silence button next to Disarm (shown while the siren sounds) backed by POST /api/v1/alarm/silence, with the same sign-in, PIN and rate limits as disarm; the central API (remote/alarm) accepts action silence too.
- The server may send a silence command (armor/alarm/<node>/command, action silence, no mode, no force, no PIN) under the same three switches as arm and disarm; the result echoes action silence.
- New event kind siren_silenced in the state message and the event list, saved at once to flash so a restart does not make the siren louder; texts, help and docs in the eight languages.
- Tests: the panel rules (silence, lockout shared with disarm, no-op when quiet, re-sound after a silence), the strict command parser and the shared samples.

## [0.3.3] - A zone that cannot be read is in trouble, not closed

- A wired zone whose converter does not start or whose loop gives no reading for five rounds in a row is in the new trouble state: it is not guarding, it never sounds the siren, it stops an arming (unless forced, and then it is left out and noted) and the server warns. Before, such a zone stayed normal for ever.
- The panel message and its documentation describe the fourth zone state.

## [0.3.2] - Only the house may use the panel, wrong passwords counted per person, and a panel that works behind the server's proxy

- **The panel answers only the house.** A new setting, *Networks that may use this panel* (`web.admin_sources`), says which addresses may sign in or change anything: empty means the private ranges of a house (10/8, 172.16/12, 192.168/16, link-local, loopback and 100.64/10, where Tailscale lives), otherwise the list of addresses and networks you write (`192.168.1.0/24, 10.66.0.0/24`). Any other address is told `outside_network`, except the public look at the node's identity, so a port opened by mistake on the router does not let the Internet try passwords here. A list that would shut out the person who is saving it is refused (`would_lock_out`).
- **Wrong passwords are counted for the person too**, not only for the address: five wrong passwords for a name slow that name down from any address (and, behind the server's proxy, where every request has the server's address, one mistyped name no longer stops everybody).
- **The panel works behind the server's proxy:** its page, its style, its script, its calls and the firmware upload use relative paths, so the same panel opens at `http://node/` and at `https://house/api/v1/nodes/<id>/panel/` (the Bluetooth and map sockets keep their own paths).
- **The fingerprint of the node's certificate is public:** `GET /api/v1/session` carries `cert_sha256`, which is what the server pins before it opens the panel of an HTTPS node; the panel already shows it in the web card so that it can be compared by hand.
- The new setting, its hint and the two new refusals are in the panel in the eight languages. Tests: 477,665 checks in the host test programs (new ones cover the list of networks, the key of a person and the setting).
- **A login or a set-up must carry the marker of the panel** (`X-Requested-With`), like every request that changes something, so a page of another site cannot sign a person in to an account it chose; the server's own sign-in to a node (update, adoption) sends it.
- **The current password of the account is counted too:** wrong ones in "change my password" are slowed down like those of a login, because a stolen session could repeat them.
- **A user change is checked before it is applied:** a role is no longer changed and then the new password refused.

## [0.3.1] - A siren that cannot be silenced, an alarm that survives a restart and sensors that cannot go quiet unseen

- **The siren's relay belongs to the alarm.** The panel's page, the broker's device commands and the central's token can no longer move it (they are told it is the siren's), it does not fall back to open when the broker is lost, and its pin is written again on every turn of the loop, so a pin that was disturbed is put back while the alarm sounds.
- **A second alarm is heard.** When the siren has timed out, or a zone that has not sounded yet goes off during an alarm, the siren sounds again (at most three times per alarm) and the event is noted; before, everything after the first alarm was ignored.
- **A restart keeps the whole alarm:** an alarm of smoke, gas or panic with the system disarmed comes back as that alarm; one whose siren had timed out comes back without the siren, and a lockout after wrong PINs comes back for as long as it lasts.
- **A reset by hand cannot wipe a guarding alarm:** the BOOT button at power-up and the factory reset of the panel are refused while the alarm is armed or ringing; a factory reset erases the saved state of the alarm and the saved events with the rest, and a button of the alarm on GPIO 0 no longer arms the reset.
- **Zigbee sensors that go quiet:** a sensor never heard since the node began to listen is judged like any other, the allowance starts again whenever the radio changes hands, a sensor is also judged through the server's feed, and one that is silent when you arm is refused (*zones open*) or, when forced, left out until you disarm instead of ringing the alarm in the middle of the exit delay.
- **A Zigbee coordinator on the network now takes over:** the node builds its own conversation also when the radio is a coordinator reached over the network; before, with one set, it never took the radio when the server went away.
- **Loop limits stop at 3000 mV:** the converter does not read above about 3.1 V, so a limit up there turned a cut wire into a contact; the settings and the panel refuse it.
- A TOGGLE from the broker is refused (a message can be delivered twice and would leave the relay the wrong way round), and the BOOT-button reset can be turned off at build time (*BOOT button erases the settings*).
- The node reads the converter with nobody else touching it; the events are written to flash at most every half minute unless they change the state of the house; the queues of the Zigbee conversation are bounded.
- HTTP answers 423 (locked out) and 408 (upload stalled) with their own status lines, and a user change that could not be written is told as a storage error.
- The panel tells every error the node can answer with in the eight languages (the updater's, the reset's, the siren's and the rest), and a test fails when a new code has no text; the radio state *through the network coordinator* has its words.
- The startup line says *alarm node*, the comments say eight languages and ESP-IDF 5.4 (the one the container builds with), and `secrets/node.conf.example` exists.
- The documents say what the node does today: the zone feed is believed by the panel, what a restart brings back, the siren's relay, the direct HTTP API, the Bluetooth pairing with its passkey, the loop's margins and the files that differ from the electrical node's.
- Shared with the other nodes: Improv over USB on a board with a cable now also tells the node to use the Wi-Fi it was given, the upload by hand is refused while an installation or a copy of the slot runs, and the headers of the signature code name the real generator script.
- The Bluetooth note of the panel no longer calls the pairing *just works*: it says what the pairing asks for and what protects it.
- Tests: 477,588 checks in six programs plus the panel texts; new ones cover a second alarm after the siren timed out, the restart of a disarmed alarm, of a silenced one and of a lockout, silent and mute Zigbee sensors, and the loop limit.
- Shared with the other nodes (from ARMOR-COMMON's firmware base): the pairing of Bluetooth closes after repeated failures, the random generator keeps away from the converter, a user write is all-or-nothing, a repeated key in a JSON document is refused and four-number firmware versions are understood.

## [0.3.0] - The last events survive a restart

- **Events kept across a restart:** the last ten events of the panel (armed, alarm, disarmed, bad PIN...) are written to flash while the node has real time and come back after a power cut, a watchdog reset or an update, told as "seconds ago" by the wall clock. Before the clock is set their age is unknown, so they are not told.
- The zone identifier field has its own help text (it showed the name of another node's field), and the four arming buttons are numbered #1 to #4.
- A host test covers writing the events, reading them back, ageing them by the wall clock and refusing what is not readable.

## [0.2.9] - A Zigbee coordinator on the network and the Bluetooth PIN in the panel

- **Signed firmware:** a node installs from GitHub only a firmware whose signature verifies. Each release carries, next to the image and its SHA-256, an Ed25519 signature of that hash (`<image>.sig`); the node downloads the image, hashes it and installs it only when the hash is the published one and the signature verifies with the public key of the releases, which is in the firmware (the private key never leaves the maintainer's computer). A release without a signature, or with a wrong one, is refused and nothing changes.
- An administrator can still upload a firmware file by hand in the panel, as before: that is the administrator's own act and is the way out if a key ever has to change.
- The panel tells the two new refusals (no signature, bad signature) in the eight languages.
- The verification (SHA-512 and Ed25519, with their constants worked out by a script instead of typed) is tested on a computer against signatures made by another implementation, with every kind of damage to the message, the signature and the key.
- An administrator can read the Bluetooth PIN in the panel (Bluetooth card, *Show the Bluetooth PIN*), without going to the USB console: it is asked of the node only then and forgotten when hidden; new route GET /api/v1/ble-pin, administrators only; the texts are in the eight languages.
- The panel can use a Zigbee coordinator on the network (SMLIGHT SLZB-06, ZigStar UZG-01...) instead of a radio on the board: new settings `zigbee.network_host` and `zigbee.network_port`, refused together with the radio of the board.
- With one, the node connects to the coordinator itself only while the server has been away (the time of *Take the radio after*), keeps guarding the Zigbee zones, and lets go of it the moment the server is back so that Zigbee2MQTT can have it; the local Zigbee settings accept either kind of radio.
- The panel has a card for it in the eight languages, host tests hold the settings (valid address, port range, conflict with the radio of the board, round trip), and the Zigbee guide explains the mode and what is not proven yet.

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
