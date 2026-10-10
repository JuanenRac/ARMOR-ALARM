// ARMOR-ALARM - the messages of an alarm node (contract version 0, see docs/ALARM_MESSAGES.md).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
//   armor/alarm/<node>/state     one message per node, every few seconds and at every change; JSON, no retained flag. What the panel is doing: its phase and mode, the siren, each zone and the last events.
//   armor/alarm/<node>/command   server -> node: arm (away or stay) or disarm. Only read when the node's setting `server.commands` and the broker's access list both allow it.
//   armor/alarm/<node>/result    node -> server: whether the command was accepted and why not; never retained.
// `node_id`, a zone id and a command id follow the rules of the whole of A.R.M.O.R.; a field that is not in the contract is refused, and a command that is not the contract's gets no answer at all.
// Nothing here touches hardware; it is tested on a computer and checked against ARMOR-COMMON's shared contract.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "alarm_panel.hpp"
#include "alarm_zone.hpp"
#include "json.hpp"
#include "node_id.hpp"

namespace armor::alarm {

inline std::string topic(const std::string& node_id, const char* leaf) {
  return node_id_is_valid(node_id) ? "armor/alarm/" + node_id + "/" + leaf : "";
}

inline const char* mode_text(Mode m) { return m == Mode::kAway ? "away" : m == Mode::kStay ? "stay" : "disarmed"; }
inline const char* event_text(EventKind k) {
  switch (k) {
    case EventKind::kArmed: return "armed"; case EventKind::kExitDelayStarted: return "exit_delay_started"; case EventKind::kEntryDelayStarted: return "entry_delay_started";
    case EventKind::kAlarm: return "alarm"; case EventKind::kSirenTimedOut: return "siren_timed_out"; case EventKind::kDisarmed: return "disarmed";
    case EventKind::kBadPin: return "bad_pin"; case EventKind::kLockedOut: return "locked_out"; case EventKind::kZoneBypassed: return "zone_bypassed";
  }
  return "armed";
}

// What the message says about one zone.
struct ZoneReport {
  std::string id, name;
  ZoneKind kind = ZoneKind::kInstant;
  ZoneState state = ZoneState::kNormal;
  bool bypassed = false;
};

constexpr std::size_t kMaxReportedEvents = 10;

// The state message. `now_ms` is the node's own clock (events are told as seconds ago); `timestamp_ms` is what goes in the message (the wall clock, or the time since start).
inline std::string state_json(const std::string& node_id, std::uint64_t timestamp_ms, std::uint64_t now_ms, const Panel& panel, bool locked_out, bool commands_enabled, const std::vector<ZoneReport>& zones) {
  json::Writer w;
  w.begin_object().field("kind", "alarm").field("node_id", node_id).key("timestamp_ms").integer(static_cast<long long>(timestamp_ms));
  w.field("phase", phase_text(panel.phase())).field("mode", mode_text(panel.mode())).field("siren", panel.siren()).field("locked_out", locked_out).field("commands_enabled", commands_enabled);
  w.key("zones").begin_array();
  for (const ZoneReport& zone : zones) {
    w.begin_object().field("id", zone.id);
    if (!zone.name.empty()) w.field("name", zone.name);
    w.field("kind", kind_text(zone.kind)).field("state", state_text(zone.state)).field("bypassed", zone.bypassed).end_object();
  }
  w.end_array();
  w.key("open_zones").begin_array();
  for (const std::string& id : panel.open_zones(panel.mode() == Mode::kStay ? Mode::kStay : Mode::kAway)) w.string(id);
  w.end_array();
  w.key("events").begin_array();
  const std::vector<Event>& events = panel.events();
  const std::size_t first = events.size() > kMaxReportedEvents ? events.size() - kMaxReportedEvents : 0;
  for (std::size_t i = first; i < events.size(); ++i) {
    w.begin_object().key("ago_s").integer(static_cast<long long>(now_ms >= events[i].at_ms ? (now_ms - events[i].at_ms) / 1000ULL : 0)).field("kind", event_text(events[i].kind));
    if (!events[i].zone.empty()) w.field("zone", events[i].zone);
    w.end_object();
  }
  w.end_array();
  w.end_object();
  return w.str();
}

// ---- the command and its answer -----------------------------------------------------------------------------------------------------

enum class Action { kArm, kDisarm };

struct Command {
  std::string command_id;   // 16 lowercase hex digits
  Action action = Action::kDisarm;
  Mode mode = Mode::kAway;  // for an arm: "away" or "stay"
  bool force = false;       // for an arm: leave out the zones that are open
};

inline bool valid_command_id(const std::string& id) {
  if (id.size() != 16) return false;
  for (const char c : id) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

// Reads a command strictly: exactly the contract's fields, for this node. False for anything else (no answer is given to it).
inline bool parse_command(const std::string& text, const std::string& node_id, Command& out) {
  json::Value document;
  if (!json::parse(text, document) || !document.is_object()) return false;
  for (const std::string& name : document.names) {
    if (name != "kind" && name != "node_id" && name != "timestamp_ms" && name != "command_id" && name != "action" && name != "mode" && name != "force") return false;
  }
  if (document.string_or("kind", "") != "alarm_command" || document.string_or("node_id", "") != node_id) return false;
  const json::Value* stamp = document.get("timestamp_ms");
  if (stamp == nullptr || !stamp->is_number() || stamp->number < 0) return false;
  Command command;
  command.command_id = document.string_or("command_id", "");
  if (!valid_command_id(command.command_id)) return false;
  const std::string action = document.string_or("action", "");
  const json::Value* mode = document.get("mode");
  const json::Value* force = document.get("force");
  if (force != nullptr && !force->is_bool()) return false;
  command.force = force != nullptr && force->boolean;
  if (action == "arm") {
    command.action = Action::kArm;
    if (mode == nullptr || !mode->is_string() || (mode->text != "away" && mode->text != "stay")) return false;   // an arm says which
    command.mode = mode->text == "stay" ? Mode::kStay : Mode::kAway;
  } else if (action == "disarm") {
    command.action = Action::kDisarm;
    if (mode != nullptr || command.force) return false;   // a disarm has nothing to say about a mode or about force
  } else return false;
  out = command;
  return true;
}

inline std::string result_json(const std::string& node_id, std::uint64_t timestamp_ms, const Command& command, Refusal refusal, Phase phase) {
  json::Writer w;
  w.begin_object().field("kind", "alarm_result").field("node_id", node_id).key("timestamp_ms").integer(static_cast<long long>(timestamp_ms));
  w.field("command_id", command.command_id).field("action", command.action == Action::kArm ? "arm" : "disarm").field("accepted", refusal == Refusal::kNone).field("refusal", refusal_text(refusal)).field("phase", phase_text(phase));
  w.end_object();
  return w.str();
}

}  // namespace armor::alarm
