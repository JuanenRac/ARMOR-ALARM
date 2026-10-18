// ARMOR-ALARM - arming profiles: the named ways of arming the panel (Away, Home, Night, a custom one). No hardware, no network: all of it is tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A profile is a small description that comes inside the arm command of the server (contract arm_profile.schema.json in ARMOR-COMMON) and says, for this one arming:
//   - the base: away guards every zone, stay leaves the interior zones out (the mode the panel reports);
//   - per zone: whether it guards (armed) and whether it alarms at once or after the entry delay (delay);
//   - the exit and the entry delay, and whether an intrusion sounds the siren (a silent alarm is still latched, recorded and told).
// What a profile can NOT do: it never switches off a smoke or panic zone (an always zone) or a tamper, it never touches the PIN, the lockout, the silence, a forced arming's refusal of a
// sounding always zone or a zone in trouble, and an arm without a profile behaves exactly as before. The node does not store profiles: the one armed is kept in flash only so that a
// restart arms the house again the same way.
#pragma once
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "alarm_zone.hpp"
#include "json.hpp"

namespace armor::alarm {

constexpr std::size_t kMaxProfileZones = 16;
constexpr int kMaxProfileDelayS = 300;

enum class ArmedOverride { kKind, kYes, kNo };           // kKind: what the base and the zone's kind say
enum class DelayOverride { kKind, kInstant, kEntry };    // kKind: what the zone's kind says

struct ZoneOverride {
  std::string id;
  ArmedOverride armed = ArmedOverride::kKind;
  DelayOverride delay = DelayOverride::kKind;
};

struct ArmProfile {
  std::string id;            // ^[a-z0-9][a-z0-9_-]{0,15}$
  std::string name;          // 1 to 32 characters
  bool stay = false;         // the base: false is away, true is stay
  int exit_delay_s = -1;     // -1: the node's own setting
  int entry_delay_s = -1;
  bool siren = true;         // false: an intrusion alarm is silent (smoke, panic and tamper always sound)
  bool notify_urgent = true; // only the server reads it; kept so that a profile read back is the profile that was given
  std::vector<ZoneOverride> zones;
};

// Characters of a UTF-8 text (the bytes that are not continuation bytes).
inline std::size_t utf8_length(const std::string& text) {
  std::size_t n = 0;
  for (const char c : text) if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
  return n;
}

inline bool valid_profile_id(const std::string& id) {
  if (id.empty() || id.size() > 16 || id.front() == '-' || id.front() == '_') return false;
  for (const char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  return true;
}

// Whether a zone of this kind guards in this arming, and whether it starts the entry delay instead of alarming at once. An always zone guards in every state whatever a profile says.
struct Resolved {
  bool armed = true;
  bool delayed = false;
};

inline Resolved resolve(ZoneKind kind, bool stay_mode, const ArmProfile* profile, const std::string& zone_id) {
  Resolved r;
  if (kind == ZoneKind::kAlways) return r;
  r.armed = kind == ZoneKind::kInterior ? !stay_mode : true;
  r.delayed = kind == ZoneKind::kEntry;
  if (profile == nullptr) return r;
  for (const ZoneOverride& item : profile->zones) {
    if (item.id != zone_id) continue;
    if (item.armed == ArmedOverride::kYes) r.armed = true;
    else if (item.armed == ArmedOverride::kNo) r.armed = false;
    if (item.delay == DelayOverride::kInstant) r.delayed = false;
    else if (item.delay == DelayOverride::kEntry) r.delayed = true;
    break;
  }
  return r;
}

// Reads a profile strictly, as the contract has it: exactly its members, each of the right type and range, a zone named once, a zone that is left out with no delay. False for anything else.
inline bool parse_profile(const json::Value& document, ArmProfile& out) {
  if (!document.is_object()) return false;
  for (const std::string& name : document.names) {
    if (name != "id" && name != "name" && name != "base" && name != "exit_delay_s" && name != "entry_delay_s" && name != "siren" && name != "notify" && name != "zones") return false;
  }
  ArmProfile profile;
  const json::Value* id = document.get("id");
  const json::Value* name = document.get("name");
  const json::Value* base = document.get("base");
  if (id == nullptr || !id->is_string() || !valid_profile_id(id->text)) return false;
  if (name == nullptr || !name->is_string() || name->text.empty() || utf8_length(name->text) > 32 || name->text.size() > 128) return false;
  if (base == nullptr || !base->is_string() || (base->text != "away" && base->text != "stay")) return false;
  profile.id = id->text;
  profile.name = name->text;
  profile.stay = base->text == "stay";
  for (const char* key : {"exit_delay_s", "entry_delay_s"}) {
    const json::Value* member = document.get(key);
    if (member == nullptr) continue;
    if (!member->is_number() || !std::isfinite(member->number) || member->number < 0 || member->number > kMaxProfileDelayS ||
        member->number != static_cast<double>(static_cast<long long>(member->number))) return false;
    (key[1] == 'x' ? profile.exit_delay_s : profile.entry_delay_s) = static_cast<int>(member->number);
  }
  const json::Value* siren = document.get("siren");
  if (siren != nullptr) { if (!siren->is_bool()) return false; profile.siren = siren->boolean; }
  const json::Value* notify = document.get("notify");
  if (notify != nullptr) {
    if (!notify->is_string() || (notify->text != "urgent" && notify->text != "normal")) return false;
    profile.notify_urgent = notify->text == "urgent";
  }
  const json::Value* zones = document.get("zones");
  if (zones != nullptr) {
    if (!zones->is_array() || zones->items.size() > kMaxProfileZones) return false;
    for (const json::Value& item : zones->items) {
      if (!item.is_object()) return false;
      for (const std::string& member : item.names) if (member != "id" && member != "armed" && member != "delay") return false;
      const json::Value* zone_id = item.get("id");
      if (zone_id == nullptr || !zone_id->is_string() || !valid_zone_id(zone_id->text)) return false;
      ZoneOverride zone;
      zone.id = zone_id->text;
      for (const ZoneOverride& seen : profile.zones) if (seen.id == zone.id) return false;
      const json::Value* armed = item.get("armed");
      if (armed != nullptr) { if (!armed->is_bool()) return false; zone.armed = armed->boolean ? ArmedOverride::kYes : ArmedOverride::kNo; }
      const json::Value* delay = item.get("delay");
      if (delay != nullptr) {
        if (!delay->is_string() || (delay->text != "instant" && delay->text != "entry")) return false;
        zone.delay = delay->text == "instant" ? DelayOverride::kInstant : DelayOverride::kEntry;
      }
      if (zone.armed == ArmedOverride::kNo && zone.delay != DelayOverride::kKind) return false;
      profile.zones.push_back(zone);
    }
  }
  out = profile;
  return true;
}

inline bool parse_profile_text(const std::string& text, ArmProfile& out) {
  json::Value document;
  return text.size() <= 2048 && json::parse(text, document) && parse_profile(document, out);
}

// The profile as the contract has it (the same text parse_profile reads back): what the flash keeps so that a restart arms the house the same way.
inline std::string profile_json(const ArmProfile& profile) {
  json::Writer w;
  w.begin_object().field("id", profile.id).field("name", profile.name).field("base", profile.stay ? "stay" : "away");
  if (profile.exit_delay_s >= 0) w.key("exit_delay_s").integer(profile.exit_delay_s);
  if (profile.entry_delay_s >= 0) w.key("entry_delay_s").integer(profile.entry_delay_s);
  w.field("siren", profile.siren).field("notify", profile.notify_urgent ? "urgent" : "normal");
  if (!profile.zones.empty()) {
    w.key("zones").begin_array();
    for (const ZoneOverride& zone : profile.zones) {
      w.begin_object().field("id", zone.id);
      if (zone.armed != ArmedOverride::kKind) w.field("armed", zone.armed == ArmedOverride::kYes);
      if (zone.delay != DelayOverride::kKind) w.field("delay", zone.delay == DelayOverride::kInstant ? "instant" : "entry");
      w.end_object();
    }
    w.end_array();
  }
  w.end_object();
  return w.str();
}

}  // namespace armor::alarm
