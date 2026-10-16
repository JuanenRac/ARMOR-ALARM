# The messages of an alarm node (contract version 0)

`armor/alarm/{node_id}/state`, `armor/alarm/{node_id}/command` and `armor/alarm/{node_id}/result`: JSON, never retained. The schemas, the topic rule and the conformance vectors are in ARMOR-COMMON (`alarm.schema.json`, `alarm_command.schema.json`, `alarm_result.schema.json`, `conformance/alarm*.json`).

## The state

One message per node, every five seconds and at every change (the phase, the mode, the siren, a zone, a lockout).

```json
{"kind":"alarm","node_id":"alarm-1","timestamp_ms":1000,"phase":"armed","mode":"away","siren":false,"locked_out":false,"commands_enabled":true,
 "zones":[{"id":"front-door","name":"Front door","kind":"entry","state":"normal","bypassed":false}],
 "open_zones":[],
 "events":[{"ago_s":12,"kind":"armed"}]}
```

- `phase`: `disarmed`, `exit_delay`, `armed`, `entry_delay` or `alarm`. `mode`: `disarmed`, `away` or `stay`.
- A zone's `kind` is `instant`, `entry`, `interior` or `always`; its `state` is `normal`, `triggered` or `tamper`; `bypassed` is true while it is left out of this arming.
- `open_zones` are the zones that would stop an arming right now. `events` are the last ten things that happened, oldest first, told as seconds ago (a node may have no clock); an event's `kind` is `armed`, `exit_delay_started`, `entry_delay_started`, `alarm`, `siren_timed_out`, `disarmed`, `bad_pin`, `locked_out` or `zone_bypassed`, with the `zone` that caused it when one did.
- `commands_enabled` says whether the node accepts the commands below at all (the setting `server.commands`).
- A field that is not in the schema is refused. The message carries a state and never a command.

## The command and the answer

```json
{"kind":"alarm_command","node_id":"alarm-1","timestamp_ms":7000,"command_id":"c0ffee0123456789","action":"arm","mode":"away","force":false}
{"kind":"alarm_result","node_id":"alarm-1","timestamp_ms":7100,"command_id":"c0ffee0123456789","action":"arm","accepted":true,"refusal":"none","phase":"exit_delay"}
```

- The actions are `arm` (with `mode`, `away` or `stay`, and an optional `force`) and `disarm`. A disarm carries no mode and no force and **no PIN**: the PIN is for whoever stands at the panel (the server checks the PIN of the signed-in user before it sends a disarm, and never passes it on). The server's authority is the broker's access list plus the node's own setting, not a secret in the message.
- A command is read **strictly**: exactly the contract's fields, for this node. Anything else (a field that is not in the contract, an arm without a mode, a disarm with one, another node's id, a command id that is not sixteen hexadecimal digits) gets no answer at all.
- The answer's `refusal` is `none` exactly when it accepted: `not_disarmed` (already armed or counting down), `zones_open` (a zone that matters is open and `force` was not given; a sounding smoke detector or panic button can never be forced), `not_armed`, `bad_pin` and `locked_out` (the last two never come to a command from the server).
- The node reads its command topic only when **both** its setting `server.commands` and the broker's access list allow it (`mqtt_identity.sh alarm-commands <node> on` in ARMOR-DEVOPS), and the server sends a command only when it has been turned on there too: three independent switches, all off by default.
- An accepted command is not an alarm that sounded: the state message says what the panel is doing.

## The Zigbee zones, told by the server

`armor/alarm/{node_id}/zigbee`: JSON, never retained, from the server to the panel, only while the server holds the Zigbee radio (Zigbee2MQTT). The panel does not hear the sensors then, so without this its Zigbee zones would stay at their last state and arming the panel would protect nothing.

```json
{"zone":"front-door","state":"triggered","battery_low":false}
```

- `state` is `normal`, `triggered` or `tamper`; `battery_low` may be left out. Nothing else is accepted.
- The server sends it every time the linked device changes and every 30 seconds meanwhile (Studio, Alarm Designer, links of Zigbee devices to zones), so a restarted panel or a lost message heals by itself.
- The panel takes it only while it does not hold the radio. When the node holds the radio it hears the sensors itself, and what the server said would be older.
- It is information, never a command: it cannot arm, disarm or sound anything. The panel decides with its own settings (the kind of zone, the delays, whether it is armed). The broker lets the node read this one topic and nothing else without an explicit `alarm-commands ... on`.
