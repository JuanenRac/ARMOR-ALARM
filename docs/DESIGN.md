# ARMOR-ALARM: what it is for and how it is built

ARMOR-ALARM is the part of A.R.M.O.R. that guards the house: the **zones** (doors, windows, motion, smoke, panic), the **arming** and its delays, the **alarm**, the **siren** and the **PIN**. It works with ARMOR-SERVER, which already knows the kinds of device an alarm needs (door, window, motion, smoke, siren, panic button) and has the notion of "armed"; and it is built to **keep guarding when the server is away** ([FAILOVER.md](FAILOVER.md)).

## What exists

- **`core/alarm_zone.hpp`:** what a zone is (four kinds, three states), how a wired loop with an end-of-line resistor becomes a state, and the debouncer.
- **`core/alarm_panel.hpp`:** the panel's rules, with no hardware and no network.
- **The node firmware** on top of them: the settings, the messages, the runner, the web panel and the Zigbee radio bridge and relays of the base board ([NODE_FIRMWARE.md](NODE_FIRMWARE.md), [ALARM_MESSAGES.md](ALARM_MESSAGES.md), [HARDWARE.md](HARDWARE.md)).

The rules are tested on a computer (`tests/test_alarm.cpp`) and so are the settings, the messages and the runner (`tests/test_node.cpp`).

## The zones

| Kind | Meaning | While armed |
|---|---|---|
| instant | a door, a window, a glass-break sensor | alarms at once |
| entry | the door you come in through | starts the entry delay |
| interior | motion inside the house | alarms in *away*, ignored in *stay* |
| always | smoke, gas, flood, panic | alarms whether armed or not |

A **tamper** (a cut or shorted loop, a sensor's own tamper switch) alarms in every state, on every kind of zone.

A wired loop gives four states on one pin: below `short_below` the loop is shorted (tamper), up to `normal_below` the contact is closed (normal), up to `triggered_below` it is open (triggered), above that the wire is cut (tamper). The limits depend on the resistors the installer chooses and are settings.

## The panel

`disarmed -> arm(away | stay) -> exit delay -> armed -> (a zone) -> alarm`, or `armed -> (an entry zone) -> entry delay -> alarm` if nobody disarms in time.

- Arming needs no PIN; disarming and silencing do. After a few wrong PINs everything is refused for a while, even the right PIN.
- Arming is refused while a zone that matters in that mode is open or tampered with, unless forced; the open zones are then left out until the disarm. A sounding always-zone is never left out.
- During the exit delay only an always-zone or a tamper alarms.
- The alarm latches until a correct disarm; the siren stops by itself after a time.
- A zone alarms when it **changes** to triggered, not while it stays so.

## What is still to do

- **The Zigbee stack** the panel needs to take the radio over while the server is away (below and in [FAILOVER.md](FAILOVER.md)): the bridge only hands the line over today.
- **The server, Studio and the Android client** reading the messages (they are in ARMOR-COMMON's contract, version 0).
- **Hardware:** the base board populated for alarm, the wired zones on the analog pins, a siren relay and a way to arm and disarm on the spot ([HARDWARE.md](HARDWARE.md) is a proposal, untried).
