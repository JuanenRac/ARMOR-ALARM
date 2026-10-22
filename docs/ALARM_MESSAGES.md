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
- A zone's `kind` is `instant`, `entry`, `interior` or `always`; its `state` is `normal`, `triggered`, `tamper` or `trouble`; `bypassed` is true while it is left out of this arming. `trouble` means that the node cannot read the zone (its converter or input gave no reading for five rounds in a row): the zone is not guarding, it never sounds the siren, it stops an arming like an open zone (and can be left out by forcing it) and the server shows a warning for as long as it lasts.
- `open_zones` are the zones that would stop an arming right now. `events` are the last ten things that happened, oldest first, told as seconds ago (a node may have no clock); an event's `kind` is `armed`, `exit_delay_started`, `entry_delay_started`, `alarm`, `siren_timed_out`, `disarmed`, `bad_pin`, `locked_out`, `zone_bypassed` or `siren_silenced` (the siren was stopped by a correct PIN while the alarm stayed latched), with the `zone` that caused it when one did.
- `commands_enabled` says whether the node accepts the commands below at all (the setting `server.commands`).
- A field that is not in the schema is refused. The message carries a state and never a command.

## The command and the answer

```json
{"kind":"alarm_command","node_id":"alarm-1","timestamp_ms":7000,"command_id":"c0ffee0123456789","action":"arm","mode":"away","force":false}
{"kind":"alarm_result","node_id":"alarm-1","timestamp_ms":7100,"command_id":"c0ffee0123456789","action":"arm","accepted":true,"refusal":"none","phase":"exit_delay"}
```

- The actions are `arm` (with `mode`, `away` or `stay`, and an optional `force`), `disarm` and `silence`. A disarm or a silence carries no mode and no force and **no PIN**: the PIN is for whoever stands at the panel (the server checks the PIN of the signed-in user before it sends either, and never passes it on). A silence stops the siren at once with the system still armed: the phase stays `alarm`, the alarm stays recorded and a zone that has not sounded in it yet may sound the siren again; a silence of a panel that is not in alarm is refused as `not_armed`, and one of a siren that is already quiet is accepted and changes nothing. On the panel's own page a silence needs the PIN and a wrong one counts toward the lockout exactly like a wrong disarm PIN. The server's authority is the broker's access list plus the node's own setting, not a secret in the message.
- A command is read **strictly**: exactly the contract's fields, for this node. Anything else (a field that is not in the contract, an arm without a mode, a disarm or a silence with one, another node's id, a command id that is not sixteen hexadecimal digits) gets no answer at all.
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
- It is information, never a command: it cannot arm or disarm. But what it says is believed: a zone that the server reports as triggered (or as tamper, which counts even disarmed) is a zone that went off, and the panel raises its alarm and sounds the siren by its own rules (the kind of zone, the delays, whether it is armed); a report of "normal" hides a sensor that the radio would say otherwise. Whoever can publish on this topic is therefore trusted with the zones, which is why the broker's access list gives it to the server's account alone. The panel also watches the feed: with `zigbee_silence_min` set, a sensor that is not heard for that long (through the feed or through the radio) is lost, and while the system is armed a lost sensor is a tamper. The broker lets the node read this one topic and nothing else without an explicit `alarm-commands ... on`.

### Signed zone messages

When the server signs its feed, each message also carries `seq` (a number that only goes up: the server's time in milliseconds) and `sig` (64 lowercase hexadecimal digits), always together:

```json
{"zone":"front-door","state":"triggered","battery_low":false,"seq":1700000000000,"sig":"330aa293a19d730d0deede3598b3efbed43887483298f15832c9ba4bfbda0378"}
```

`sig` is the HMAC-SHA256 over `node_id|zone|state|battery_low (1 or 0)|seq` with the key of this panel (64 hexadecimal digits that an administrator takes from Studio and types in the panel, Delays and PIN page; it is stored like the PIN and never shown again). A panel that holds the key refuses a message whose signature is wrong or whose `seq` is not above the last one it took, and says so in its log. `tests/test_policy.cpp` holds the value above, which ARMOR-COMMON and ARMOR-SERVER compute too.

## The server's security options

`armor/alarm/{node_id}/policy`: JSON, never retained, from the server to the panel (said again every thirty seconds): `{"config_hash":true,"zone_feed":true,"zone_feed_signature":false}`, exactly those three flags. The panel keeps the last one it heard in flash, so it applies it from its first moment after a restart, and until the server has spoken it does what it always did.

- `config_hash`: the saved armed state is tied to a hash of the zones (which, where, how) and the PIN it was saved under, and is not given back after a restart if either changed (the panel then starts disarmed and says why in its log). A state saved before the option was on has no hash and comes back as before.
- `zone_feed`: off, and the messages above are ignored.
- `zone_feed_signature`: on, and an unsigned message, or a signed one that cannot be checked because the panel holds no key, is refused.

Each of the three can also be fixed on the node itself (Delays and PIN page, "Security options of the server"): *Follow the server* (the default), *On* or *Off*, whatever the server says. A signature required on the node needs the key to be stored.

## The arming profile

An arm may carry a `profile` (the `arm_profile` message of ARMOR-COMMON), chosen by the server from the modes the owner keeps (Away, Home, Night, their own). The node stores none of them: the one in the command is used for that arming, and a copy is kept in flash only so that a restart arms the house the same way. The state message says that the node understands profiles (`arm_profiles: true`, the server sends a profile only to a node that says so) and, while armed with one, its id (`profile`).

A profile changes, for that arming only:

* **which zones guard:** `armed` per zone. By default a zone guards by its kind and the base (`base: stay` leaves the interior zones out, `away` guards all); a profile may switch an interior zone on or any other zone off. A zone that is left out and merely triggered no longer stops the arming.
* **when a zone alarms:** `delay: instant` alarms at once, `delay: entry` starts the entry delay first, whatever the zone's kind says.
* **the delays:** `exit_delay_s` and `entry_delay_s` replace the node's own for that arming (0 is allowed).
* **the siren:** `siren: false` makes an intrusion a silent alarm: the alarm latches, is recorded and reported, and nothing sounds. Smoke, panic (always zones) and a tamper still sound, and a siren that is already sounding is never switched off by a silent zone.

A profile never touches the PIN, the lockout, the silence of the siren, the refusal of an arming over a sounding always-zone or a tamper, a zone in trouble (it still stops the arming unless it is forced), nor a tamper on a zone the profile leaves out. An arm without a profile behaves exactly as before: `tests/test_profiles.cpp` runs the same random scenario on a panel armed plainly and on one armed with a profile that says nothing but its base, and they must agree at every step. The base of the profile must be the mode of the command; a command whose profile is not exactly the contract's is not understood and gets no answer, like any other malformed command. `notify` is read only by the server.
