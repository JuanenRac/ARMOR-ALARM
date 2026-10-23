// ARMOR-NODE-ALARM - what a zone is, and how a wired loop read by the ADC becomes a state.
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
// kTrouble: the node cannot read the zone (its converter or its input does not answer). The zone is not guarding: it never sounds the siren, and it stops an arming unless it is left out.
enum class ZoneState { kNormal, kTriggered, kTamper, kTrouble };

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
inline const char* state_text(ZoneState state) { return state == ZoneState::kTriggered ? "triggered" : state == ZoneState::kTamper ? "tamper" : state == ZoneState::kTrouble ? "trouble" : "normal"; }

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

inline bool limits_are_valid(const LoopLimits& l) { return l.short_below_mv > 0 && l.short_below_mv < l.normal_below_mv && l.normal_below_mv < l.triggered_below_mv && l.triggered_below_mv <= 3000; }

inline ZoneState classify_loop(double millivolts, const LoopLimits& limits) {
  if (millivolts < limits.short_below_mv) return ZoneState::kTamper;
  if (millivolts < limits.normal_below_mv) return ZoneState::kNormal;
  if (millivolts < limits.triggered_below_mv) return ZoneState::kTriggered;
  return ZoneState::kTamper;
}

// A plain contact on a digital pin (a switch to ground, the pin pulled up by the node or by the board): closed pulls the pin low, open lets it go high. A contact that is closed while
// all is well (`normally_closed`: a door contact, a tamper switch) is triggered when it opens; one that is open while all is well (a panic button, the dry output of a sensor) is
// triggered when it closes. A tamper switch (`tamper_role`) says tamper instead of triggered, which raises the alarm in every state of the system.
inline ZoneState classify_contact(bool pin_high, bool normally_closed, bool tamper_role) {
  const bool triggered = normally_closed ? pin_high : !pin_high;
  return !triggered ? ZoneState::kNormal : tamper_role ? ZoneState::kTamper : ZoneState::kTriggered;
}

// A push button on a digital pin, pressed when the pin is low: it fires ONCE per press, after the level has held for `hold_ms` (a button that bounces fires once), and again only after
// it has been let go.
class ButtonFilter {
 public:
  explicit ButtonFilter(std::uint64_t hold_ms = 50) : hold_ms_(hold_ms) {}
  bool feed(bool pressed, std::uint64_t now_ms) {
    if (!pressed) { held_ = false; fired_ = false; return false; }
    if (!held_) { held_ = true; since_ms_ = now_ms; }
    if (!fired_ && now_ms - since_ms_ >= hold_ms_) { fired_ = true; return true; }
    return false;
  }
 private:
  std::uint64_t hold_ms_, since_ms_ = 0;
  bool held_ = false, fired_ = false;
};

// A reading must hold for `hold_ms` before it counts, so that a contact that bounces, or a wire that a gust of wind shakes, does not raise the alarm. The first one too, starting from "normal".
class Debouncer {
 public:
  explicit Debouncer(std::uint64_t hold_ms = 200) : hold_ms_(hold_ms) {}
  ZoneState feed(ZoneState reading, std::uint64_t now_ms) {
    // The first reading has to hold like any other: a converter that is still settling at power-up gave a false tamper the moment the node restarted. The zone reads "normal" for those
    // first 200 ms; a door that was really open is reported just after.
    if (!started_) { started_ = true; stable_ = ZoneState::kNormal; candidate_ = reading; since_ms_ = now_ms; return stable_; }
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
