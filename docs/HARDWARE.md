# The hardware: what is proposed and what is not decided

Nothing here has been built or connected. The alarm node is the electrical node's base board populated for alarm: the same pins, the same module (see [BASE_BOARD.md](BASE_BOARD.md)).

## A wired zone: a loop with an end-of-line resistor (a proposal, untried)

One analog pin (ADC1, GPIO 1 to 10; the range read is 0 to about 3.1 V) tells four states of a loop:

```
 3.3 V --[ Rup 4.7 k ]--+--[ 1 k ]-- pin (with 100 nF to ground and a clamp to 3.3 V and ground)
                        |
                        +-- Re 2.2 k --+-- contact (normally closed) --+--> to ground
                                        |                               |
                                        +------ Rpar 4.7 k -------------+
```

| The loop | Resistance to ground | At the pin (3.3 V) | State |
|---|---|---|---|
| a short | 0 | about 0 V | tamper |
| contact closed | Re = 2.2 k | about 1.05 V | normal |
| contact open | Re + Rpar = 6.9 k | about 1.96 V | triggered |
| a cut wire | nothing | about 3.3 V (reads about 3.1 V) | tamper |

The firmware's default limits (400, 1200 and 2400 mV) fit this circuit, but the margin between the closed contact (about 1.05 V) and the first limit (1.2 V) is only about 150 mV, which the tolerance of 5 % resistors and the converter's error can eat: with these resistors set the limits halfway between the states (for example 500, 1500 and 2600 mV), and measure the real values on the *Zones* page before trusting them. The last limit stops at 3000 mV: the converter does not read above about 3.1 V, and a cut wire must read above the last limit to be a tamper. They are settings and the *Zones* page shows what each pin reads, which is how to check the wiring and to choose the limits for other resistors. A smoke detector or a sensor with a dry output uses the same loop with its relay contact in place of the door's.

- **Never put more than 3.3 V on a pin.** The series resistor, the capacitor and the clamp diodes (for example a BAT54S) protect the pin from what a long cable picks up. Keep the cable away from mains.
- **Pins:** the zones use the analog pins of the base board (GPIO 1, 2, 4, 5, 6 and 7 by default); the relay pins are free for the siren and other outputs, or - populated as inputs instead - for a tamper switch on the box or the buttons of a keypad.

## The siren

A relay of the base board (any of the eight, named in the panel) closes while the alarm sounds. The siren's own supply is the installer's (a 12 V siren from a battery that is kept charged); the relay's driver and contacts must be rated for it, with a flyback diode on an inductive load. Every relay starts open when the node starts; an alarm that was ringing when the node restarted rings again (see [SAFETY.md](SAFETY.md)), unless its siren had already timed out. The siren's relay belongs to the alarm: the panel's page, the broker and the central cannot move it, it does not fall back to open when the broker is lost, and its pin is written again on every turn of the loop.

The relays of the base board can also be interlocked in pairs (a relay names another that must never be closed together with it; a request to close one while the other is closed is refused, `relay_interlocked`). The siren is the exception: the alarm closes its relay whatever an interlock says.

## Power

Without power there is no alarm. The PoE switch, the router or access point and the panel need a battery backup (a UPS) for the alarm to survive a power cut; the siren's battery too. The node's supply is the PoE of its Ethernet cable.

## To decide

- How many wired zones, and whether the node has a keypad, a screen or only a button and a buzzer for arming on the spot.
- Whether more inputs and outputs are needed (an I2C expander takes two pins).
- Where the Zigbee sensors' receiving node stands (the radio's antenna wants a free side of the box), and which nodes are routers of the same Zigbee network.
