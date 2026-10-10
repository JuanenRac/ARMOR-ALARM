// ARMOR-ALARM - what a zone is, and how a wired loop read by the ADC becomes a state.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A zone is one thing that can raise the alarm: a wired loop (a contact with an end-of-line resistor, read as a voltage), or a Zigbee sensor the panel hears about (a door or
// window contact, a motion sensor, a smoke detector, a panic button). The kind of a zone says WHEN it matters:
//   instant   a door, a window, a glass-break sensor on the perimeter: alarms at once while the system is armed;
//   entry     the door you come in through: while armed, it starts the entry delay instead of alarming at once;
//   interior  motion inside the house: alarms only in "away" (armed with nobody home), is ignored in "stay";
//   always    smoke, gas, flood, panic: alarms whether armed or not (a "24 hour" zone).
// Nothing here touches hardware or the network, so all of it is tested on a computer. This is not a certified alarm system.
#pragma once
#include <cstdint>
#include <string>

namespace armor::alarm {

enum class ZoneKind { kInstant, kEntry, kInterior, kAlways };

// What a zone says now. `kTamper` is a cut or shorted loop or a sensor's own tamper switch: it raises the alarm in every state of the system.
enum class ZoneState { kNormal, kTriggered, kTamper };

struct Zone {
  std::string id;               // lowercase letters, digits, - and _ (the rule of the whole of A.R.M.O.R.)
  std::string name;
  ZoneKind kind = ZoneKind::kInstant;
};

inline const char* kind_text(ZoneKind kind) {
  switch (kind) { case ZoneKind::kInstant: return "instant"; case ZoneKind::kEntry: return "entry"; case ZoneKind::kInterior: return "interior"; case ZoneKind::kAlways: return "always"; }
  return "instant";
}
inline bool parse_kind(const std::string& text, ZoneKind& out) {
  for (ZoneKind k : {ZoneKind::kInstant, ZoneKind::kEntry, ZoneKind::kInterior, ZoneKind::kAlways}) if (text == kind_text(k)) { out = k; return true; }
  return false;
}
inline const char* state_text(ZoneState state) { return state == ZoneState::kTriggered ? "triggered" : state == ZoneState::kTamper ? "tamper" : "normal"; }

inline bool valid_zone_id(const std::string& id) {
  if (id.empty() || id.size() > 32 || id.front() == '-' || id.front() == '_') return false;
  for (const char c : id) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  return true;
}

// A wired loop with an end-of-line resistor, read by the ADC as a voltage at the pin. The three limits are in millivolts and ascending:
//   below `short_below`      the loop is shorted                    -> tamper
//   up to `normal_below`     the contact is closed (the resistor)   -> normal
//   up to `triggered_below`  the contact is open (the second value) -> triggered
//   above it                 the wire is cut (nothing pulls it)     -> tamper
// What the three values are depends on the resistors the installer chooses; they are settings, not constants.
struct LoopLimits {
  double short_below_mv = 400;
  double normal_below_mv = 1200;
  double triggered_below_mv = 2400;
};

inline bool limits_are_valid(const LoopLimits& l) { return l.short_below_mv > 0 && l.short_below_mv < l.normal_below_mv && l.normal_below_mv < l.triggered_below_mv && l.triggered_below_mv <= 3300; }

inline ZoneState classify_loop(double millivolts, const LoopLimits& limits) {
  if (millivolts < limits.short_below_mv) return ZoneState::kTamper;
  if (millivolts < limits.normal_below_mv) return ZoneState::kNormal;
  if (millivolts < limits.triggered_below_mv) return ZoneState::kTriggered;
  return ZoneState::kTamper;
}

// A reading must hold for `hold_ms` before it counts, so that a contact that bounces, or a wire that a gust of wind shakes, does not raise the alarm. The first reading is taken
// as it is: a node that has just started knows nothing better.
class Debouncer {
 public:
  explicit Debouncer(std::uint64_t hold_ms = 200) : hold_ms_(hold_ms) {}
  ZoneState feed(ZoneState reading, std::uint64_t now_ms) {
    if (!started_) { started_ = true; stable_ = reading; candidate_ = reading; since_ms_ = now_ms; return stable_; }
    if (reading != candidate_) { candidate_ = reading; since_ms_ = now_ms; }
    if (candidate_ != stable_ && now_ms - since_ms_ >= hold_ms_) stable_ = candidate_;
    return stable_;
  }
  ZoneState state() const { return stable_; }
 private:
  std::uint64_t hold_ms_;
  bool started_ = false;
  ZoneState stable_ = ZoneState::kNormal, candidate_ = ZoneState::kNormal;
  std::uint64_t since_ms_ = 0;
};

}  // namespace armor::alarm
