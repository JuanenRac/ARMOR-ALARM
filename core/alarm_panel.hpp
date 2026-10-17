// ARMOR-ALARM - the rules of the panel: arming, the exit and entry delays, the alarm, the siren and the PIN. No hardware, no network: all of it is tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The panel is a state machine fed with the state of each zone and with the requests of whoever is at the keypad (or of the server):
//   disarmed -> arm(away | stay) -> exit delay -> armed -> (a zone) -> alarm            ...or armed -> (an entry zone) -> entry delay -> alarm if nobody disarms in time
// Rules worth knowing:
//   - Arming needs no PIN (anyone may leave); DISARMING does, and so does silencing the siren. Wrong PINs are counted: after `lockout_after` of them in a row disarming is refused for
//     `lockout_s` seconds, even the right PIN.
//   - Arming is refused while a zone that matters in that mode is open or tampered with, unless `force` is given: the open zones are then left out until the system is disarmed (an
//     always-zone, smoke or panic, that is sounding can never be left out).
//   - During the exit delay only an always-zone or a tamper raises the alarm: the person leaving is not an intruder.
//   - The alarm LATCHES: only a correct disarm ends it. A correct PIN can also silence the siren at once with the system still armed (the alarm stays latched and recorded). The siren stops by itself after `siren_max_s` (the alarm stays latched, the neighbours do not have to put up with it); a zone that
//     has not sounded in this alarm yet sounds it again, at most three times.
//   - A zone raises the alarm when it CHANGES to triggered (or tamper), not while it stays so: a smoke detector that is still sounding when you disarm does not restart the alarm until it
//     has been quiet and sounds again. At the moment of arming the zones are looked at as they are (see above).
//   - Always-zones (smoke, panic) and a tamper alarm in every state, armed or not.
// This is not a certified alarm system, and these rules have never run on a real installation.
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "alarm_zone.hpp"

namespace armor::alarm {

enum class Mode { kDisarmed, kAway, kStay };
enum class Phase { kDisarmed, kExitDelay, kArmed, kEntryDelay, kAlarm };
enum class Refusal { kNone, kNotDisarmed, kZonesOpen, kBadPin, kLockedOut, kNotArmed };
enum class EventKind { kArmed, kExitDelayStarted, kEntryDelayStarted, kAlarm, kSirenTimedOut, kDisarmed, kBadPin, kLockedOut, kZoneBypassed, kSirenSilenced };

struct Timing {
  std::uint32_t exit_delay_s = 30;
  std::uint32_t entry_delay_s = 30;
  std::uint32_t siren_max_s = 180;
  std::uint32_t lockout_after = 3;   // wrong PINs in a row
  std::uint32_t lockout_s = 60;
  // A Zigbee sensor that says nothing for this many minutes is taken as lost (a flat battery, a jammed radio): while the system is armed it counts as tamper, because a dead sensor is how
  // a perimeter is defeated. 0 turns it off (a sensor that only reports when it changes would otherwise be taken for lost).
  std::uint32_t zigbee_silence_min = 0;
};

struct Event {
  std::uint64_t at_ms = 0;
  EventKind kind = EventKind::kArmed;
  std::string zone;   // the zone that caused it, when one did
  std::int64_t epoch = 0;   // the wall-clock second it happened at, 0 while the node had no real time
  bool restored = false;    // read back from flash after a restart: its age comes from `epoch`, not from the node's own clock
};

inline const char* phase_text(Phase p) {
  switch (p) { case Phase::kDisarmed: return "disarmed"; case Phase::kExitDelay: return "exit_delay"; case Phase::kArmed: return "armed"; case Phase::kEntryDelay: return "entry_delay"; case Phase::kAlarm: return "alarm"; }
  return "disarmed";
}
inline const char* refusal_text(Refusal r) {
  switch (r) { case Refusal::kNone: return "none"; case Refusal::kNotDisarmed: return "not_disarmed"; case Refusal::kZonesOpen: return "zones_open"; case Refusal::kBadPin: return "bad_pin";
               case Refusal::kLockedOut: return "locked_out"; case Refusal::kNotArmed: return "not_armed"; }
  return "none";
}

class Panel {
 public:
  static constexpr std::size_t kMaxEvents = 100;

  Panel(const Timing& timing, std::vector<Zone> zones, std::string pin) : timing_(timing), pin_(std::move(pin)) {
    for (Zone& zone : zones) { Slot slot; slot.zone = std::move(zone); slots_.push_back(std::move(slot)); }
  }

  // ---- what the panel is doing ----
  Phase phase() const { return phase_; }
  Mode mode() const { return mode_; }
  bool siren() const { return siren_on_; }
  bool locked_out(std::uint64_t now_ms) const { return locked_until_ms_ != 0 && now_ms < locked_until_ms_; }
  const std::vector<Event>& events() const { return events_; }
  // The wall-clock second now (0 while the node has no real time): the events noted from here on carry it, and the ones restored from flash are aged by it.
  void set_epoch(std::int64_t epoch) { epoch_ = epoch; }
  std::int64_t epoch() const { return epoch_; }
  // The events a restart brought back from flash go before the ones of this run.
  void restore_events(std::vector<Event> older) {
    for (Event& event : older) event.restored = true;
    events_.insert(events_.begin(), older.begin(), older.end());
    if (events_.size() > kMaxEvents) events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(events_.size() - kMaxEvents));
  }
  // The zones that would stop an arming in `mode` right now.
  std::vector<std::string> open_zones(Mode mode) const {
    std::vector<std::string> out;
    for (const Slot& s : slots_) if (blocks_arming(s, mode)) out.push_back(s.zone.id);
    return out;
  }
  bool bypassed(const std::string& id) const { const Slot* s = find(id); return s != nullptr && s->bypassed; }
  ZoneState zone_state(const std::string& id) const { const Slot* s = find(id); return s != nullptr ? s->state : ZoneState::kNormal; }

  // ---- what happens to it ----

  // A zone's state, as the loop or the sensor says it (already debounced). An unknown zone is ignored.
  void set_zone(const std::string& id, ZoneState state, std::uint64_t now_ms) {
    Slot* slot = find(id);
    if (slot == nullptr) return;
    const ZoneState before = slot->state;
    slot->state = state;
    if (state != before && state != ZoneState::kNormal && state != ZoneState::kTrouble) react(*slot, now_ms);   // a zone that cannot be read tells nothing: it never sounds
  }

  Refusal arm(Mode mode, bool force, std::uint64_t now_ms) {
    if (mode == Mode::kDisarmed) return Refusal::kNotArmed;
    if (phase_ != Phase::kDisarmed) return Refusal::kNotDisarmed;
    const std::vector<std::string> open = open_zones(mode);
    if (!open.empty() && !force) return Refusal::kZonesOpen;
    // A smoke detector or a panic button that is sounding is never left out, forced or not: it is the one thing that must be looked at first.
    // A cut or shorted loop (tamper) is never left out either: forcing an arming must not hide a sabotage.
    for (const std::string& id : open) if (find(id)->zone.kind == ZoneKind::kAlways || find(id)->state == ZoneState::kTamper) return Refusal::kZonesOpen;
    for (Slot& s : slots_) s.bypassed = false;
    for (const std::string& id : open) { find(id)->bypassed = true; note(now_ms, EventKind::kZoneBypassed, id); }
    mode_ = mode;
    phase_ = Phase::kExitDelay;
    phase_since_ms_ = now_ms;
    note(now_ms, EventKind::kExitDelayStarted, "");
    if (timing_.exit_delay_s == 0) finish_exit(now_ms);
    return Refusal::kNone;
  }

  // Ends the armed state or an alarm. Needs the PIN.
  Refusal disarm(const std::string& pin, std::uint64_t now_ms) {
    const Refusal pin_result = check_pin(pin, now_ms);
    if (pin_result != Refusal::kNone) return pin_result;
    if (phase_ == Phase::kDisarmed) return Refusal::kNone;
    phase_ = Phase::kDisarmed;
    mode_ = Mode::kDisarmed;
    siren_on_ = false;
    alarmed_.clear();
    resounds_ = 0;
    for (Slot& s : slots_) s.bypassed = false;
    note(now_ms, EventKind::kDisarmed, "");
    return Refusal::kNone;
  }

  // Stops the siren at once with the system still armed: the alarm stays latched and recorded, and a zone that has not sounded in it yet may sound the siren again. Needs the PIN, and a
  // wrong one counts toward the lockout exactly like a wrong disarm PIN. Without a ringing alarm there is nothing to silence (kNotArmed); a siren already quiet stays so.
  Refusal silence(const std::string& pin, std::uint64_t now_ms) {
    const Refusal pin_result = check_pin(pin, now_ms);
    if (pin_result != Refusal::kNone) return pin_result;
    return silence_now(now_ms);
  }

  // The same on the SERVER's say-so (see disarm_authorized): no PIN, not held back by a lockout.
  Refusal silence_authorized(std::uint64_t now_ms) {
    wrong_pins_ = 0;
    return silence_now(now_ms);
  }

  // Ends the armed state or an alarm on the SERVER's say-so (a command that came over the broker, which the node accepted only because both its own setting and the broker's access list allow
  // it): no PIN, and not held back by a lockout. A panel that is already disarmed stays so.
  Refusal disarm_authorized(std::uint64_t now_ms) {
    wrong_pins_ = 0;
    if (phase_ == Phase::kDisarmed) return Refusal::kNone;
    phase_ = Phase::kDisarmed;
    mode_ = Mode::kDisarmed;
    siren_on_ = false;
    alarmed_.clear();
    resounds_ = 0;
    for (Slot& s : slots_) s.bypassed = false;
    note(now_ms, EventKind::kDisarmed, "");
    return Refusal::kNone;
  }

  // Time passes: the exit and entry delays run out, the siren times out, a lockout ends.
  void tick(std::uint64_t now_ms) {
    if (phase_ == Phase::kExitDelay && now_ms - phase_since_ms_ >= static_cast<std::uint64_t>(timing_.exit_delay_s) * 1000ULL) finish_exit(now_ms);
    if (phase_ == Phase::kEntryDelay && now_ms - phase_since_ms_ >= static_cast<std::uint64_t>(timing_.entry_delay_s) * 1000ULL) raise(now_ms, entry_zone_);
    if (phase_ == Phase::kAlarm && siren_on_ && now_ms - alarm_since_ms_ >= static_cast<std::uint64_t>(timing_.siren_max_s) * 1000ULL) { siren_on_ = false; note(now_ms, EventKind::kSirenTimedOut, ""); }
    if (locked_until_ms_ != 0 && now_ms >= locked_until_ms_) { locked_until_ms_ = 0; }
  }

  // ---- what a restart must not forget ----
  // A power cut, a watchdog reset or an update must not leave the house unguarded: the node keeps the mode, whether an alarm was running and the zones left out, and takes them back at
  // start. An exit delay that was running comes back as armed; an entry delay that was running (somebody was inside, not yet disarmed) comes back as an alarm.
  struct Saved {
    Phase phase = Phase::kDisarmed;
    Mode mode = Mode::kDisarmed;
    std::vector<std::string> bypassed;
    bool silenced = false;   // the alarm was running but its siren had timed out
    std::uint32_t lockout = 0;   // 0 none, 1..3 the lockout that was running (its length grows with it)
    bool operator==(const Saved& other) const { return phase == other.phase && mode == other.mode && bypassed == other.bypassed && silenced == other.silenced && lockout == other.lockout; }
  };
  Saved saved() const {
    Saved out;
    if (locked_until_ms_ != 0) out.lockout = lockouts_ < 1 ? 1 : lockouts_;
    if (phase_ == Phase::kDisarmed) return out;
    out.silenced = phase_ == Phase::kAlarm && !siren_on_;
    out.mode = mode_;
    out.phase = (phase_ == Phase::kAlarm || phase_ == Phase::kEntryDelay) ? Phase::kAlarm : (phase_ == Phase::kDisarmed ? Phase::kDisarmed : Phase::kArmed);
    for (const Slot& s : slots_) if (s.bypassed) out.bypassed.push_back(s.zone.id);
    return out;
  }
  // Takes the saved state back. Returns true when the panel is not disarmed afterwards.
  bool restore(const Saved& saved, std::uint64_t now_ms) {
    // a lockout that was running comes back for as long as it lasts (the clock starts again with the node)
    if (saved.lockout > 0 && saved.lockout <= 3 && locked_until_ms_ == 0) {
      static constexpr std::uint32_t kGrowth[3] = {1, 5, 15};
      lockouts_ = saved.lockout;
      locked_until_ms_ = now_ms + static_cast<std::uint64_t>(timing_.lockout_s) * kGrowth[saved.lockout - 1] * 1000ULL;
    }
    if (phase_ != Phase::kDisarmed || saved.phase == Phase::kDisarmed) return false;
    // an alarm of an always-zone (smoke, panic) or of a tamper runs with the system disarmed: it comes back as the same alarm, not as nothing
    if (saved.mode == Mode::kDisarmed && saved.phase != Phase::kAlarm) return false;
    for (Slot& s : slots_) s.bypassed = false;
    for (const std::string& id : saved.bypassed) if (Slot* s = find(id)) s->bypassed = true;
    mode_ = saved.mode;
    if (saved.mode != Mode::kDisarmed) {
      phase_ = Phase::kArmed;
      phase_since_ms_ = now_ms;
      note(now_ms, EventKind::kArmed, "");
    }
    if (saved.phase == Phase::kAlarm) {
      raise(now_ms, "");
      if (saved.silenced) siren_on_ = false;   // it had been silenced by the timeout: a restart does not make it louder
    }
    return true;
  }

 private:
  struct Slot {
    Zone zone;
    ZoneState state = ZoneState::kNormal;
    bool bypassed = false;
  };

  Slot* find(const std::string& id) { for (Slot& s : slots_) if (s.zone.id == id) return &s; return nullptr; }
  const Slot* find(const std::string& id) const { for (const Slot& s : slots_) if (s.zone.id == id) return &s; return nullptr; }

  // Does this zone, in this state, stop the system from being armed in `mode`? An interior zone does not matter in "stay"; a bypassed one never does.
  static bool blocks_arming(const Slot& s, Mode mode) {
    if (s.bypassed || s.state == ZoneState::kNormal) return false;
    if (s.state == ZoneState::kTriggered && s.zone.kind == ZoneKind::kInterior && mode == Mode::kStay) return false;
    return true;
  }

  bool pin_matches(const std::string& pin) const {
    unsigned char difference = static_cast<unsigned char>(pin.size() != pin_.size());
    const std::size_t n = std::min(pin.size(), pin_.size());
    for (std::size_t i = 0; i < n; ++i) difference |= static_cast<unsigned char>(pin[i] ^ pin_[i]);
    return difference == 0 && !pin_.empty();
  }

  void note(std::uint64_t at, EventKind kind, const std::string& zone) {
    if (events_.size() >= kMaxEvents) events_.erase(events_.begin());
    events_.push_back({at, kind, zone, epoch_, false});
  }

  void finish_exit(std::uint64_t now_ms) {
    phase_ = Phase::kArmed;
    phase_since_ms_ = now_ms;
    note(now_ms, EventKind::kArmed, "");
    // a zone that was already triggered when the delay ended (and was not left out) raises its alarm now, as it would have had it changed a moment later
    for (Slot& s : slots_) if (!s.bypassed && s.state != ZoneState::kNormal && s.state != ZoneState::kTrouble) react(s, now_ms);
  }

  // The PIN rules shared by disarming and silencing: a lockout refuses even the right PIN, wrong PINs are counted, a right one clears the count.
  Refusal check_pin(const std::string& pin, std::uint64_t now_ms) {
    if (locked_out(now_ms)) return Refusal::kLockedOut;
    if (!pin_matches(pin)) {
      note(now_ms, EventKind::kBadPin, "");
      if (++wrong_pins_ >= timing_.lockout_after) {
        // Each lockout without a right PIN in between lasts longer: once, five times and fifteen times `lockout_s` (a fixed minute let somebody try three PINs a minute for ever).
        static constexpr std::uint32_t kGrowth[3] = {1, 5, 15};
        const std::uint32_t factor = kGrowth[lockouts_ < 2 ? lockouts_ : 2];
        if (lockouts_ < 2) ++lockouts_;
        locked_until_ms_ = now_ms + static_cast<std::uint64_t>(timing_.lockout_s) * factor * 1000ULL;
        wrong_pins_ = 0;
        note(now_ms, EventKind::kLockedOut, "");
      }
      return Refusal::kBadPin;
    }
    wrong_pins_ = 0;
    lockouts_ = 0;
    return Refusal::kNone;
  }

  Refusal silence_now(std::uint64_t now_ms) {
    if (phase_ != Phase::kAlarm) return Refusal::kNotArmed;
    if (!siren_on_) return Refusal::kNone;
    siren_on_ = false;
    note(now_ms, EventKind::kSirenSilenced, "");
    return Refusal::kNone;
  }

  void raise(std::uint64_t now_ms, const std::string& zone) {
    if (phase_ == Phase::kAlarm) {
      // The alarm is latched, but a zone that has not sounded yet (or any trigger once the siren has timed out) is news: the siren sounds again, at most kMaxResounds times, and it is noted.
      const bool fresh = !zone.empty() && std::find(alarmed_.begin(), alarmed_.end(), zone) == alarmed_.end();
      if ((!fresh && siren_on_) || resounds_ >= kMaxResounds) return;
      ++resounds_;
      if (fresh) alarmed_.push_back(zone);
      alarm_since_ms_ = now_ms;
      siren_on_ = true;
      note(now_ms, EventKind::kAlarm, zone);
      return;
    }
    phase_ = Phase::kAlarm;
    alarm_since_ms_ = now_ms;
    siren_on_ = true;
    resounds_ = 0;
    alarmed_.clear();
    if (!zone.empty()) alarmed_.push_back(zone);
    note(now_ms, EventKind::kAlarm, zone);
  }

  // A zone has just changed to triggered or tamper (or was found so when the exit delay ended).
  void react(Slot& s, std::uint64_t now_ms) {
    if (s.bypassed && s.state != ZoneState::kTamper) return;   // a zone left out ignores its contact, never its sabotage
    if (s.state == ZoneState::kTamper || s.zone.kind == ZoneKind::kAlways) { raise(now_ms, s.zone.id); return; }   // in every state, even disarmed
    switch (phase_) {
      case Phase::kDisarmed: case Phase::kExitDelay: return;
      case Phase::kAlarm:   // already ringing: a zone that guards in this mode and has not sounded yet sounds the siren again (raise decides)
        if (mode_ != Mode::kDisarmed && (s.zone.kind == ZoneKind::kInstant || s.zone.kind == ZoneKind::kEntry || (s.zone.kind == ZoneKind::kInterior && mode_ == Mode::kAway))) raise(now_ms, s.zone.id);
        return;
      case Phase::kEntryDelay:
        if (s.zone.kind == ZoneKind::kInstant || (s.zone.kind == ZoneKind::kInterior && mode_ == Mode::kAway)) raise(now_ms, s.zone.id);
        return;
      case Phase::kArmed:
        if (s.zone.kind == ZoneKind::kInstant) raise(now_ms, s.zone.id);
        else if (s.zone.kind == ZoneKind::kInterior) { if (mode_ == Mode::kAway) raise(now_ms, s.zone.id); }
        else if (s.zone.kind == ZoneKind::kEntry) { phase_ = Phase::kEntryDelay; phase_since_ms_ = now_ms; entry_zone_ = s.zone.id; note(now_ms, EventKind::kEntryDelayStarted, s.zone.id); if (timing_.entry_delay_s == 0) raise(now_ms, s.zone.id); }
        return;
    }
  }

  Timing timing_;
  std::string pin_;
  std::vector<Slot> slots_;
  std::vector<Event> events_;
  std::int64_t epoch_ = 0;
  Phase phase_ = Phase::kDisarmed;
  Mode mode_ = Mode::kDisarmed;
  bool siren_on_ = false;
  std::uint64_t phase_since_ms_ = 0, alarm_since_ms_ = 0, locked_until_ms_ = 0;
  std::uint32_t wrong_pins_ = 0, lockouts_ = 0;
  std::string entry_zone_;
  static constexpr std::uint32_t kMaxResounds = 3;
  std::uint32_t resounds_ = 0;
  std::vector<std::string> alarmed_;   // the zones that have sounded in this alarm
};

// The saved state as one line the flash can hold: "armed|away|front-door,window" (the phase, the mode, the zones left out). Anything that is not exactly that is not a saved state
// (a damaged record never arms a panel at random).
inline std::string saved_text(const Panel::Saved& saved) {
  std::string text = std::string(phase_text(saved.phase)) + "|" + (saved.mode == Mode::kAway ? "away" : saved.mode == Mode::kStay ? "stay" : "disarmed") + "|";
  for (std::size_t i = 0; i < saved.bypassed.size(); ++i) text += (i ? "," : "") + saved.bypassed[i];
  // optional extras after a third bar: "silenced" and "lock1".."lock3"
  if (saved.silenced || saved.lockout > 0) {
    text += "|";
    if (saved.silenced) text += "silenced";
    if (saved.lockout > 0) text += std::string(saved.silenced ? "," : "") + "lock" + std::to_string(saved.lockout);
  }
  return text;
}
inline bool parse_saved(const std::string& text, Panel::Saved& out) {
  const std::size_t first = text.find('|'), second = first == std::string::npos ? first : text.find('|', first + 1);
  if (second == std::string::npos || text.size() > 400) return false;
  const std::size_t third = text.find('|', second + 1);
  if (third != std::string::npos && text.find('|', third + 1) != std::string::npos) return false;
  Panel::Saved saved;
  const std::string phase = text.substr(0, first), mode = text.substr(first + 1, second - first - 1);
  if (phase == "armed") saved.phase = Phase::kArmed; else if (phase == "alarm") saved.phase = Phase::kAlarm; else if (phase == "disarmed") saved.phase = Phase::kDisarmed; else return false;
  if (mode == "away") saved.mode = Mode::kAway; else if (mode == "stay") saved.mode = Mode::kStay; else if (mode == "disarmed") saved.mode = Mode::kDisarmed; else return false;
  if ((saved.phase == Phase::kDisarmed) != (saved.mode == Mode::kDisarmed) && saved.phase != Phase::kAlarm) return false;
  const std::size_t listed_end = third == std::string::npos ? text.size() : third;
  std::string id;
  for (std::size_t i = second + 1; i <= listed_end; ++i) {
    if (i == listed_end || text[i] == ',') { if (!id.empty()) saved.bypassed.push_back(id); id.clear(); }
    else id += text[i];
  }
  if (third != std::string::npos) {
    std::string word;
    for (std::size_t i = third + 1; i <= text.size(); ++i) {
      if (i == text.size() || text[i] == ',') {
        if (word == "silenced") saved.silenced = true;
        else if (word == "lock1") saved.lockout = 1;
        else if (word == "lock2") saved.lockout = 2;
        else if (word == "lock3") saved.lockout = 3;
        else if (!word.empty()) return false;
        word.clear();
      } else word += text[i];
    }
  }
  out = saved;
  return true;
}

}  // namespace armor::alarm
