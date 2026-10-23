# Keeping guard when the server is away

## Who is in charge

| Situation | Who decides |
|---|---|
| Everything works | ARMOR-SERVER, with Zigbee2MQTT. The radio's node is a cable. |
| The server, or the network to it, is down; the nodes are up | The panel (this project), with its own copy of the alarm's settings |
| The server is back | The server again; the panel hands the radio back (what it logged stays in its own log: the server does not collect it yet) |
| The radio's node or the radio is down | Nobody, unless there is a second radio (a possible later addition) |

## The topology

- The **nodes** are on Ethernet with PoE: the electrical node's base board populated for alarm (a Zigbee radio, wired zones, a siren relay). They only lend the radio's serial line over TCP (the *Zigbee bridge* of ARMOR-NODE-ELECTRICAL).
- The **panel** (the alarm's brain when the server is away) may be a separate unit on Wi-Fi, with a keypad or a screen, a siren and a battery.
- **One Zigbee network:** a single coordinator, with the other PoE nodes as routers that extend the mesh (to be confirmed with the real modules); several coordinators would be several networks.

## The node takes the radio itself

The simplest panel is the node: when the server's Zigbee2MQTT has not been connected to the bridge for the time set in the panel (*Take the radio after*), the node holds the radio itself and talks Zigbee to the coordinator (core/local_zigbee.hpp): it brings it up, reads the sensors of the zones that name one and commands the Zigbee sirens, feeding the same panel as the wired loops. The moment the server's client connects again the node lets go. This is written and tested against a stand-in coordinator; it has never run against a real one.

## The relay: the panel takes the radio over

The bridge accepts two clients: the **primary** (the server running Zigbee2MQTT) and the **fallback** (the panel). The fallback is accepted only while the primary is not connected and has been away for a configurable time (30 seconds by default); the primary may always connect and takes the radio back. This is built and tested in ARMOR-NODE-ELECTRICAL (`zigbee.fallback_client`, `zigbee.fallback_after_s`).

Once it has the radio the panel talks to the Zigbee coordinator itself. It needs only a small part of Zigbee: the coordinator's serial protocol (Z-Stack) and the standard alarm clusters - **IAS Zone** (door, window, motion, smoke, panic) and **IAS WD** (a siren). It does not need a catalogue of devices. The Zigbee network (its keys and its devices) lives in the coordinator, so the sensors keep reporting without pairing again. To be proved on the real modules: the hand-over with Zigbee2MQTT, and that the sensors' reports reach whoever holds the radio.

## The panel's copy of the settings

Without the server the panel needs to know which sensor is which zone, the delays, the PIN, which zones are always-zones and which siren sounds. Today the panel has its own copy, set in its own page (and by the server's Alarm Designer through the node's API); the server does not yet push the settings by itself each time they change - that is a proposal.

## How the panel and the nodes talk without the server

If the server also hosts the broker, the broker is away too. The proposal is a direct, authenticated HTTP API between the panel and each node: the wired zones go up to the panel, the siren and relay commands go down to the node. The node's side of this exists: a direct HTTP API with a token (`/api/v1/remote/state`, `/relay` and `/alarm`) reads the zones, moves the relays that are not the siren's and arms and disarms the panel with the same rules as its page, and the base board's relays can be moved by the broker (off by default, three independent switches) and fall back to open when the link is lost - except the siren's relay, which only the alarm moves. What is not written is the separate panel that would use that API when the broker is away too.

## Power

The PoE switch, the router or access point and the panel must be on a battery backup. Without it, none of this survives a power cut, whatever the firmware does.
