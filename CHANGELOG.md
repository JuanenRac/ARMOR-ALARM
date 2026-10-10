# Changelog

All notable changes to this project are documented here.

## [0.0.1] - The core of the alarm: zones, the loop and the panel's rules

- **Zones** (`core/alarm_zone.hpp`): four kinds - instant, entry, interior and always - and three states (normal, triggered, tamper). A zone may be a wired loop or a Zigbee sensor.
- **The wired loop:** a contact with an end-of-line resistor, read as a voltage, gives normal, triggered, or tamper for a shorted or a cut wire; the three limits are settings and are checked (ascending, within the converter's range). A debouncer makes a reading hold before it counts, so a bouncing contact does not raise the alarm.
- **The panel** (`core/alarm_panel.hpp`): arming in *away* and *stay*, an exit delay in which only an always-zone or a tamper alarms, an entry delay started by an entry zone, an alarm that latches until a correct disarm, a siren that stops by itself after a time, bypass of open zones when arming is forced (never a sounding always-zone), a zone that alarms only when it *changes* to triggered (a smoke detector still sounding after a disarm does not restart the alarm), and a log of the last hundred events.
- **The PIN:** arming needs none; disarming and silencing do. Wrong PINs are counted and after a few everything is refused for a while, even the right PIN; the comparison does not depend on where the PINs differ, and a panel with no PIN set never disarms.
- **Tests:** scripted scenarios for every rule and a long random run (60 rounds of 3,000 steps) that checks what must always hold - a siren means an alarm, an alarm ends only with a correct disarm, a wrong PIN never disarms, a quiet history keeps a disarmed panel quiet, the entry delay never overruns - 492,806 checks in all; a broken PIN check and a siren that never times out are both caught.
- **Documents:** the design, how the panel takes over from the server (`docs/FAILOVER.md`) and the safety notes. There is no firmware, no Zigbee stack and no message with the server yet; nothing has run on a board.
