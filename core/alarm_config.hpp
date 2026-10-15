// ARMOR-ALARM - the settings of an alarm node: what the panel's web page edits, what is stored in flash and what the firmware obeys.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// What every node has (identity, the way in, the broker, the clock...) is core/node_settings.hpp's, and the base board's Zigbee radio, relays and whether the broker may move them are
// core/io_config.hpp's: the same files the electrical node uses. This file adds what makes it an ALARM: up to eight wired zones (a loop with an end-of-line resistor on an analog pin), the
// limits that tell the states of a loop apart, the panel's delays and its PIN, which relay is the siren, and whether the server may arm and disarm it. Pure: no hardware, all tested on a computer.
// The PIN is a secret like a password: a document sent without one keeps the stored one, and "pin_set" tells the panel that there is one.
#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "alarm_panel.hpp"
#include "alarm_zone.hpp"
#include "base_board.hpp"
#include "board_s3.hpp"
#include "config_common.hpp"
#include "io_config.hpp"
#include "local_zigbee.hpp"
#include "node_settings.hpp"

namespace armor::config {

constexpr std::size_t kZoneCount = board::kAnalogCount;   // a wired zone is read by an analog pin, so there are as many as the board has

// One wired zone: a loop on an analog pin (`input` "loop": an end-of-line resistor, four states) or a plain contact on a digital pin (`input` "digital": a switch to ground, two states,
// `contact` "nc" or "no", and `role` "contact" or "tamper" for the switch of a box). `kind` says when it matters (alarm::ZoneKind); the reading must hold `hold_ms` to count (the debouncer).
struct ZoneSetting {
  bool enabled = false;
  std::string id;           // lowercase letters, digits, - and _ : what the zone is called in the messages
  std::string name;         // what a person calls it ("Front door")
  int pin = -1;             // wired: the analog pin of its loop, or the digital pin of its contact
  std::string input = "loop";     // "loop" (analog, end-of-line resistor) or "digital" (a contact to ground, the pin pulled up)
  std::string contact = "nc";      // digital: "nc" (closed while all is well, triggered when it opens) or "no" (open while all is well, triggered when it closes)
  std::string role = "contact";    // digital: "contact" (triggered) or "tamper" (the switch of a box: says tamper)
  std::string zigbee;       // a Zigbee sensor instead: its long (IEEE) address as 16 hexadecimal digits; the pin is then not used
  alarm::ZoneKind kind = alarm::ZoneKind::kInstant;
  int hold_ms = 200;
  bool is_zigbee() const { return !zigbee.empty(); }
  bool is_digital() const { return !is_zigbee() && input == "digital"; }
};

// A push button that arms the panel (no PIN: anyone may leave; disarming is only ever done with the PIN): a switch to ground on a digital pin, the pin pulled up.
constexpr std::size_t kButtonCount = 4;
struct ButtonSetting {
  bool enabled = false;
  std::string action = "arm_away";   // "arm_away" or "arm_stay"
  int pin = -1;
};

// The node taking the radio over: after the server's Zigbee2MQTT has been away for `after_s` seconds (0: never) the node speaks Zigbee itself, reads the sensors of the zones that name one and
// commands the sirens listed here (long addresses). It needs the radio's bridge (zigbee.enabled). Back to the server the moment its client connects again.
struct ZigbeeLocal {
  int after_s = 0;
  std::array<std::string, 4> sirens;
};

// The panel: its delays, the PIN and which relay is the siren (a relay of the base board, by its name; empty: no siren).
struct PanelSetting {
  alarm::Timing timing;
  std::string pin;
  std::string siren_relay;
};

// Whether the server may arm and disarm the panel by a command (armor/alarm/<node>/command). OFF by default: the panel then answers only to its own page. On, the broker's access list must
// allow it too (ARMOR-DEVOPS, mqtt_identity.sh alarm-commands <node> on): both are needed, and the server has its own switch.
struct ServerLink {
  bool commands = false;
};

struct Settings : NodeSettings, IoSettings {
  std::array<ZoneSetting, kZoneCount> zones;
  std::array<ButtonSetting, kButtonCount> buttons;
  alarm::LoopLimits loop;
  PanelSetting panel;
  ServerLink server;
  ZigbeeLocal zigbee_local;
};

// ---- the defaults of a node that has never been configured ---------------------------------------------------------------------

// `mac_tail` is the last three bytes of the node's MAC as six lowercase hexadecimal digits: it makes the first identity unique.
inline Settings default_settings(std::string_view mac_tail) {
  Settings s;
  node_defaults(s, mac_tail);
  io_defaults(s);
  for (std::size_t i = 0; i < kZoneCount; ++i) { s.zones[i].id = "zone" + std::to_string(i + 1); s.zones[i].pin = board::kAnalogPins[i]; }
  return s;
}

inline bool pin_is_valid(std::string_view pin) {
  if (pin.size() < 4 || pin.size() > 12) return false;
  for (const char c : pin) if (c < '0' || c > '9') return false;
  return true;
}

// ---- reading -------------------------------------------------------------------------------------------------------------------

// Applies the members present in `document` on top of `s`; whatever the document omits stays as it was. The result is checked with validate(): read_settings() only reports a member of the
// wrong type or size.
inline void read_settings(const json::Value& document, Settings& s, Problems& problems) {
  using namespace detail;
  if (!document.is_object()) { bad(problems, "", "invalid"); return; }
  node_read(document, s, problems);
  io_read(document, s, problems);
  if (const json::Value* zones = document.get("zones"); zones != nullptr) {
    if (!zones->is_array() || zones->items.size() > kZoneCount) bad(problems, "zones", "invalid");
    else for (std::size_t i = 0; i < zones->items.size(); ++i) {
      const json::Value& item = zones->items[i];
      const std::string base = "zones." + std::to_string(i) + ".";
      if (!item.is_object()) { bad(problems, base + "enabled", "invalid"); continue; }
      ZoneSetting& zone = s.zones[i];
      read_bool(item, "enabled", zone.enabled, base + "enabled", problems);
      read_text(item, "id", zone.id, 32, base + "id", problems);
      read_text(item, "name", zone.name, 40, base + "name", problems);
      read_int(item, "pin", zone.pin, -1, board::kLastGpio, base + "pin", problems);
      read_text(item, "zigbee", zone.zigbee, 16, base + "zigbee", problems);
      read_text(item, "input", zone.input, 8, base + "input", problems);
      read_text(item, "contact", zone.contact, 2, base + "contact", problems);
      read_text(item, "role", zone.role, 8, base + "role", problems);
      read_int(item, "hold_ms", zone.hold_ms, 20, 5000, base + "hold_ms", problems);
      if (const json::Value* kind = item.get("kind"); kind != nullptr) {
        if (!kind->is_string() || !alarm::parse_kind(kind->text, zone.kind)) bad(problems, base + "kind", "invalid");
      }
    }
  }
  if (const json::Value* buttons = document.get("buttons"); buttons != nullptr) {
    if (!buttons->is_array() || buttons->items.size() > kButtonCount) bad(problems, "buttons", "invalid");
    else for (std::size_t i = 0; i < buttons->items.size(); ++i) {
      const json::Value& item = buttons->items[i];
      const std::string base = "buttons." + std::to_string(i) + ".";
      if (!item.is_object()) { bad(problems, base + "enabled", "invalid"); continue; }
      ButtonSetting& button = s.buttons[i];
      read_bool(item, "enabled", button.enabled, base + "enabled", problems);
      read_text(item, "action", button.action, 10, base + "action", problems);
      read_int(item, "pin", button.pin, -1, board::kLastGpio, base + "pin", problems);
    }
  }
  if (const json::Value* loop = document.get("loop"); loop != nullptr && loop->is_object()) {
    read_double(*loop, "short_below_mv", s.loop.short_below_mv, 0.0, 3300.0, "loop.short_below_mv", problems);
    read_double(*loop, "normal_below_mv", s.loop.normal_below_mv, 0.0, 3300.0, "loop.normal_below_mv", problems);
    read_double(*loop, "triggered_below_mv", s.loop.triggered_below_mv, 0.0, 3300.0, "loop.triggered_below_mv", problems);
  }
  if (const json::Value* panel = document.get("panel"); panel != nullptr && panel->is_object()) {
    alarm::Timing& t = s.panel.timing;
    int value = 0;
    value = static_cast<int>(t.exit_delay_s); read_int(*panel, "exit_delay_s", value, 0, 600, "panel.exit_delay_s", problems); t.exit_delay_s = static_cast<std::uint32_t>(value);
    value = static_cast<int>(t.entry_delay_s); read_int(*panel, "entry_delay_s", value, 0, 600, "panel.entry_delay_s", problems); t.entry_delay_s = static_cast<std::uint32_t>(value);
    value = static_cast<int>(t.siren_max_s); read_int(*panel, "siren_max_s", value, 1, 3600, "panel.siren_max_s", problems); t.siren_max_s = static_cast<std::uint32_t>(value);
    value = static_cast<int>(t.lockout_after); read_int(*panel, "lockout_after", value, 1, 20, "panel.lockout_after", problems); t.lockout_after = static_cast<std::uint32_t>(value);
    value = static_cast<int>(t.lockout_s); read_int(*panel, "lockout_s", value, 1, 3600, "panel.lockout_s", problems); t.lockout_s = static_cast<std::uint32_t>(value);
    read_secret(*panel, "pin", s.panel.pin, "panel.pin", problems);
    read_text(*panel, "siren_relay", s.panel.siren_relay, 32, "panel.siren_relay", problems);
  }
  if (const json::Value* local = document.get("zigbee_local"); local != nullptr && local->is_object()) {
    read_int(*local, "after_s", s.zigbee_local.after_s, 0, 3600, "zigbee_local.after_s", problems);
    if (const json::Value* sirens = local->get("sirens"); sirens != nullptr) {
      if (!sirens->is_array() || sirens->items.size() > s.zigbee_local.sirens.size()) bad(problems, "zigbee_local.sirens", "invalid");
      else {
        for (std::string& siren : s.zigbee_local.sirens) siren.clear();
        for (std::size_t i = 0; i < sirens->items.size(); ++i) {
          if (!sirens->items[i].is_string()) { bad(problems, "zigbee_local.sirens." + std::to_string(i), "invalid"); continue; }
          s.zigbee_local.sirens[i] = sirens->items[i].text;
        }
      }
    }
  }
  if (const json::Value* server = document.get("server"); server != nullptr && server->is_object()) read_bool(*server, "commands", s.server.commands, "server.commands", problems);
}

// ---- checking ------------------------------------------------------------------------------------------------------------------

inline Problems validate(const Settings& s) {
  using detail::bad;
  Problems problems;
  node_validate(s, problems);

  // the pins: the wired zones' and the base board's may not be reserved nor shared
  detail::PinClaims claims;
  const auto claim = [&](int gpio, const std::string& who, const std::string& path) {
    if (!board::assignable(gpio)) { bad(problems, path, "reserved"); return; }
    if (!claims.claim(gpio, who)) bad(problems, path, "conflict");
  };
  io_validate(s, s.mqtt.enabled, claim, problems);

  // the zones: each has its own name and its own analog pin
  std::vector<std::string> ids;
  std::vector<std::uint64_t> addresses;
  std::size_t enabled = 0, loops = 0;
  for (std::size_t i = 0; i < kZoneCount; ++i) {
    const ZoneSetting& zone = s.zones[i];
    if (!zone.enabled) continue;
    ++enabled;
    const std::string base = "zones." + std::to_string(i) + ".";
    if (!alarm::valid_zone_id(zone.id)) bad(problems, base + "id", zone.id.empty() ? "required" : "invalid");
    else if (std::find(ids.begin(), ids.end(), zone.id) != ids.end()) bad(problems, base + "id", "conflict");
    else ids.push_back(zone.id);
    std::size_t characters = 0;
    if (!zone.name.empty() && (!net::valid_utf8(zone.name, &characters) || characters > 40)) bad(problems, base + "name", "invalid");
    if (zone.is_zigbee()) {
      std::uint64_t ieee = 0;
      if (!zigbee::parse_ieee(zone.zigbee, ieee)) bad(problems, base + "zigbee", "invalid");
      else if (std::find(addresses.begin(), addresses.end(), ieee) != addresses.end()) bad(problems, base + "zigbee", "conflict");
      else addresses.push_back(ieee);
      if (s.zigbee_local.after_s == 0) bad(problems, base + "zigbee", "needs_local");
      continue;
    }
    if (zone.input != "loop" && zone.input != "digital") bad(problems, base + "input", "invalid");
    if (zone.is_digital()) {
      // a plain contact on any pin the board lets be an input (the relay pins left unused are the natural place)
      if (zone.contact != "nc" && zone.contact != "no") bad(problems, base + "contact", "invalid");
      if (zone.role != "contact" && zone.role != "tamper") bad(problems, base + "role", "invalid");
      if (zone.pin < 0) bad(problems, base + "pin", "required");
      else if (!board::assignable(zone.pin)) bad(problems, base + "pin", "reserved");
      else if (!claims.claim(zone.pin, "zone" + std::to_string(i + 1))) bad(problems, base + "pin", "conflict");
      continue;
    }
    ++loops;
    if (zone.pin < 0) bad(problems, base + "pin", "required");
    else if (!board::analog_capable(zone.pin)) bad(problems, base + "pin", board::assignable(zone.pin) ? "not_analog" : "reserved");
    else if (!claims.claim(zone.pin, "zone" + std::to_string(i + 1))) bad(problems, base + "pin", "conflict");
  }
  for (std::size_t i = 0; i < kButtonCount; ++i) {
    const ButtonSetting& button = s.buttons[i];
    if (!button.enabled) continue;
    const std::string base = "buttons." + std::to_string(i) + ".";
    if (button.action != "arm_away" && button.action != "arm_stay") bad(problems, base + "action", "invalid");
    if (button.pin < 0) bad(problems, base + "pin", "required");
    else if (!board::assignable(button.pin)) bad(problems, base + "pin", "reserved");
    else if (!claims.claim(button.pin, "button" + std::to_string(i + 1))) bad(problems, base + "pin", "conflict");
  }
  if (loops > 0 && !alarm::limits_are_valid(s.loop)) bad(problems, "loop.short_below_mv", "range");

  // the panel: a PIN of four to twelve digits as soon as there is anything to guard, and a siren that is one of the relays
  if (!s.panel.pin.empty() && !pin_is_valid(s.panel.pin)) bad(problems, "panel.pin", "invalid");
  if (enabled > 0 && s.panel.pin.empty()) bad(problems, "panel.pin", "required");
  if (!s.panel.siren_relay.empty()) {
    bool found = false;
    for (const RelaySetting& relay : s.relays) if (relay.enabled && relay.name == s.panel.siren_relay) found = true;
    if (!found) bad(problems, "panel.siren_relay", "unknown_relay");
  }
  if (s.server.commands && !s.mqtt.enabled) bad(problems, "server.commands", "needs_broker");
  if (s.zigbee_local.after_s != 0 && (s.zigbee_local.after_s < 10 || !s.zigbee.enabled)) bad(problems, "zigbee_local.after_s", s.zigbee.enabled ? "range" : "needs_zigbee");
  for (std::size_t i = 0; i < s.zigbee_local.sirens.size(); ++i) {
    std::uint64_t ieee = 0;
    if (!s.zigbee_local.sirens[i].empty() && !zigbee::parse_ieee(s.zigbee_local.sirens[i], ieee)) bad(problems, "zigbee_local.sirens." + std::to_string(i), "invalid");
  }
  return problems;
}

// ---- writing -------------------------------------------------------------------------------------------------------------------

// `secrets`: true writes the passwords and the PIN (for flash storage); false replaces them with "password_set" and "pin_set" flags (for the panel).
inline std::string to_json(const Settings& s, bool secrets) {
  json::Writer w;
  w.begin_object();
  node_write_head(w, s, secrets);
  w.key("zones").begin_array();
  for (const ZoneSetting& zone : s.zones) {
    w.begin_object().field("enabled", zone.enabled).field("id", zone.id).field("name", zone.name).field("pin", zone.pin).field("zigbee", zone.zigbee).field("input", zone.input).field("contact", zone.contact).field("role", zone.role).field("kind", alarm::kind_text(zone.kind)).field("hold_ms", zone.hold_ms).end_object();
  }
  w.end_array();
  w.key("buttons").begin_array();
  for (const ButtonSetting& button : s.buttons) w.begin_object().field("enabled", button.enabled).field("action", button.action).field("pin", button.pin).end_object();
  w.end_array();
  w.key("loop").begin_object().key("short_below_mv").number(s.loop.short_below_mv).key("normal_below_mv").number(s.loop.normal_below_mv).key("triggered_below_mv").number(s.loop.triggered_below_mv).end_object();
  w.key("panel").begin_object().field("exit_delay_s", static_cast<long long>(s.panel.timing.exit_delay_s)).field("entry_delay_s", static_cast<long long>(s.panel.timing.entry_delay_s))
      .field("siren_max_s", static_cast<long long>(s.panel.timing.siren_max_s)).field("lockout_after", static_cast<long long>(s.panel.timing.lockout_after))
      .field("lockout_s", static_cast<long long>(s.panel.timing.lockout_s)).field("siren_relay", s.panel.siren_relay);
  if (secrets) w.field("pin", s.panel.pin); else w.field("pin_set", !s.panel.pin.empty());
  w.end_object();
  w.key("server").begin_object().field("commands", s.server.commands).end_object();
  w.key("zigbee_local").begin_object().field("after_s", s.zigbee_local.after_s).key("sirens").begin_array();
  for (const std::string& siren : s.zigbee_local.sirens) if (!siren.empty()) w.string(siren);
  w.end_array().end_object();
  io_write(w, s, secrets);
  node_write_tail(w, s);
  w.end_object();
  return w.str();
}

// Reads a stored or received document on top of `base` and checks the result. Nothing is applied unless `problems` stays empty.
inline bool load(std::string_view text, const Settings& base, Settings& out, Problems& problems) {
  json::Value document;
  if (!json::parse(text, document)) { problems.push_back({"", "not_json"}); return false; }
  out = base;
  read_settings(document, out, problems);
  if (problems.empty()) problems = validate(out);
  return problems.empty();
}

}  // namespace armor::config
