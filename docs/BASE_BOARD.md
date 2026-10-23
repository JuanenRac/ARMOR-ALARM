# The base board of the alarm node

The alarm node uses **the electrical node's base board**, populated for alarm: the same ESP32-S3 module, the same pins, a different set of parts. Everything about the board - the pin map, the Zigbee module (Ebyte E72-2G4M20S1E) and its wiring, the relay outputs, the analog inputs and the checklist for the first power-up - is in ARMOR-NODE-ELECTRICAL's `docs/BASE_BOARD.md`; the pins are the defaults of `core/base_board.hpp`, which both projects carry as the same file.

What changes with the alarm populated:

| Pins | Electrical node | Alarm node |
|---|---|---|
| Analog pins (GPIO 1, 2, 4, 5, 6, 7) | voltage and current inputs | the wired zones (an end-of-line loop each, see [HARDWARE.md](HARDWARE.md)) |
| Relay pins (GPIO 38 to 42, 47, 48, 21) | relay drivers | one is the siren's driver; the others stay relays or become inputs (a tamper switch, a keypad's buttons) |
| UART1 (GPIO 16 and 15) | the meters' serial line | not used |
| Zigbee radio (UART2, GPIO 43 and 44 for its control lines) | populated | populated |

A pin has one meaning on the board whichever parts are populated; each firmware uses its own subset. If a pin has to change when the board is designed, change the constants of `core/base_board.hpp` (and the same file in ARMOR-NODE-ELECTRICAL).
