// ARMOR-NODE-ALARM - how the node is wired together from its settings, decided in one place and tested on a computer: how each zone is read (an analog loop, a digital contact, a Zigbee
// sensor, not at all), whether the converter is needed, whether the node may take the Zigbee radio itself, which topics of the broker it listens to, and which relay is the siren (that relay
// then belongs to the alarm alone). The task in main/alarm_manager.cpp asks this file and does what it says; nothing here touches the hardware.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <array>
#include <string>
#include <vector>

#include "alarm_config.hpp"
#include "alarm_message.hpp"

namespace armor::alarm {

// How a zone is read. A zone that is not enabled is not read; a wired zone is read by the converter (a loop) or by a digital pin (a contact); a Zigbee zone has no pin at all.
enum class ZoneInput { kOff, kAnalogLoop, kDigital, kZigbee };

inline const char* to_text(ZoneInput input) {
  switch (input) {
    case ZoneInput::kOff: return "off";
    case ZoneInput::kAnalogLoop: return "loop";
    case ZoneInput::kDigital: return "digital";
    case ZoneInput::kZigbee: return "zigbee";
  }
  return "off";
}

inline ZoneInput input_of(const config::ZoneSetting& zone) {
  if (!zone.enabled) return ZoneInput::kOff;
  if (zone.is_zigbee()) return ZoneInput::kZigbee;
  if (zone.is_digital()) return ZoneInput::kDigital;
  return ZoneInput::kAnalogLoop;
}

// The Zigbee radio can be the node's own only when it has a radio (on the board or a coordinator on the network) and `after_s` says when to take it (0: never).
inline bool takes_the_radio(const config::Settings& s) { return s.zigbee_local.after_s != 0 && (s.zigbee.enabled || !s.zigbee.network_host.empty()); }

struct Wiring {
  std::array<ZoneInput, config::kZoneCount> zones{};
  std::array<bool, config::kButtonCount> buttons{};   // a button that arms the panel is read
  bool needs_converter = false;                       // at least one analog loop: the converter is started
  bool takes_radio = false;                           // the node creates the host of the Zigbee radio
  unsigned radio_after_s = 0;
  bool listens_zone_feed = true;                      // the server's information about the Zigbee zones: read even when the server may not command the panel
  bool listens_policy = true;                         // the options the server keeps for the panel
  bool listens_command = false;                       // only when the settings let the server arm, disarm and silence
  int siren_relay = -1;                               // the place of the siren relay among the relays, or -1
  bool siren_relay_unknown = false;                   // a siren relay is named, but there is no such enabled relay: there is no siren
  bool reserve_siren_relay = false;                   // the relay is the alarm's alone: neither the page nor the server's relay orders may move it

  std::size_t zones_read(ZoneInput input) const {
    std::size_t n = 0;
    for (const ZoneInput z : zones) if (z == input) ++n;
    return n;
  }

  // The topics of the broker the node listens to, in the order it subscribes.
  std::vector<std::string> topics(const std::string& node_id) const {
    std::vector<std::string> out;
    if (listens_zone_feed) out.push_back(topic(node_id, "zigbee"));
    if (listens_policy) out.push_back(topic(node_id, "policy"));
    if (listens_command) out.push_back(topic(node_id, "command"));
    return out;
  }
};

// `index_of(name)` is the place of the enabled relay with that name among the relays, or -1 (relay_outputs::index_of on the node).
template <typename IndexOf>
Wiring plan_wiring(const config::Settings& s, IndexOf index_of) {
  Wiring w;
  for (std::size_t i = 0; i < config::kZoneCount; ++i) {
    w.zones[i] = input_of(s.zones[i]);
    if (w.zones[i] == ZoneInput::kAnalogLoop) w.needs_converter = true;
  }
  for (std::size_t i = 0; i < config::kButtonCount; ++i) w.buttons[i] = s.buttons[i].enabled;
  w.takes_radio = takes_the_radio(s);
  w.radio_after_s = w.takes_radio ? static_cast<unsigned>(s.zigbee_local.after_s) : 0;
  w.listens_command = s.server.commands;
  if (!s.panel.siren_relay.empty()) {
    w.siren_relay = index_of(s.panel.siren_relay);
    w.siren_relay_unknown = w.siren_relay < 0;
    w.reserve_siren_relay = w.siren_relay >= 0;
  }
  return w;
}

}  // namespace armor::alarm
