// ARMOR-ALARM - what the alarm task decides around the panel, without the hardware: what comes back from flash at start, what must be written to flash and when, and when the state is said again.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The task in main/alarm_manager.cpp only reads the pins, gives this the clock, writes what it asks to be written, puts the siren's level on its relay and talks to the broker. The rules are here and are
// tested on a computer:
//   - at start: the saved state comes back armed unless the option ties it to the configuration and the zones or the PIN changed (alarm_policy.hpp); a record that cannot be read is ignored;
//   - while running: the state is written when it changes; the hash it was saved under goes with it (or, when the option is turned on later, alone); the arming profile follows the arming; the events are
//     written at once when something that matters happened and at most every half minute otherwise (a wrong PIN or a bouncing sensor must not wear the flash);
//   - the state message is made again when the panel's phase, mode, siren, lockout, profile or any zone changed, and anyway every few seconds.
// Nothing here writes anything: a write is asked for (Writes) and told back once done, so a write that failed is asked for again on the next turn.
#pragma once
#include <cstdint>
#include <string>

#include "alarm_config.hpp"
#include "alarm_message.hpp"
#include "alarm_panel.hpp"
#include "alarm_policy.hpp"
#include "alarm_runner.hpp"
#include "arm_profile.hpp"

namespace armor::alarm {

constexpr std::uint64_t kPublishEveryMs = 5000;       // the state is said again this often, and at every change
constexpr std::uint64_t kEventsWriteEveryMs = 30000;  // events that do not change the state of the house are written at most this often

// What flash held at start ("" for what was not there or could not be read).
struct Stored {
  std::string state, profile, policy, hash, events;
};

// What the start decided, for the log.
enum class BootResult { kNothingSaved, kRestored, kNotRestoredConfigChanged, kStateUnreadable, kStateNotArmed };
inline const char* boot_text(BootResult r) {
  switch (r) {
    case BootResult::kNothingSaved: return "nothing_saved";
    case BootResult::kRestored: return "restored";
    case BootResult::kNotRestoredConfigChanged: return "config_changed";
    case BootResult::kStateUnreadable: return "unreadable";
    case BootResult::kStateNotArmed: return "not_armed";
  }
  return "nothing_saved";
}

// What is to be written to flash now. An empty `state`, `policy` or `events` means "nothing to write"; the profile can be written empty (no profile), so it has its own flag.
struct Writes {
  std::string state;          // the one line of saved_text
  bool bind_hash = false;     // after the state, write the hash of the configuration it was saved under
  bool hash_only = false;     // the option was turned on after the state was saved: write the hash alone
  bool profile = false;       // write `profile_text` ("" erases the profile)
  std::string profile_text;
  std::string events;
  bool any() const { return !state.empty() || hash_only || profile || !events.empty(); }
};

class Supervisor {
 public:
  explicit Supervisor(const config::Settings& settings) : runner_(settings), config_hash_(config_fingerprint(settings)) {
    saved_state_ = saved_text(runner_.saved());   // a panel that starts disarmed
  }

  Runner& runner() { return runner_; }
  const Runner& runner() const { return runner_; }
  const std::string& config_hash() const { return config_hash_; }

  // ---- start ----
  // Takes back what flash held. The policy first (it decides whether the hash counts), then the state, the profile and the events.
  BootResult boot(const Stored& stored, std::uint64_t now_ms) {
    ArmProfile profile;
    const bool have_profile = !stored.profile.empty() && parse_profile_text(stored.profile, profile);
    Policy policy;
    if (!stored.policy.empty() && parse_policy(stored.policy, policy)) { runner_.set_policy(policy); saved_policy_ = policy_text(policy); }
    saved_hash_ = stored.hash;
    BootResult result = BootResult::kNothingSaved;
    Panel::Saved saved;
    if (!stored.state.empty() && parse_saved(stored.state, saved)) {
      if (!may_restore(runner_.effective().config_hash, saved_hash_, config_hash_)) {
        result = BootResult::kNotRestoredConfigChanged;
        saved_state_.clear();   // flash still holds the old state: the next turn writes the disarmed one over it
      } else if (runner_.restore(saved, now_ms, have_profile ? &profile : nullptr)) {
        result = BootResult::kRestored;
        saved_state_ = saved_text(runner_.saved());
      } else {
        result = BootResult::kStateNotArmed;
      }
    } else if (!stored.state.empty()) {
      result = BootResult::kStateUnreadable;
    }
    // What flash holds of the profile: the one armed with, or whatever was there and nobody uses (then the next turn erases it, so that a later arming without a profile cannot bring it back).
    saved_profile_ = runner_.profile() != nullptr ? profile_json(*runner_.profile()) : stored.profile;
    if (!stored.events.empty()) { saved_events_ = stored.events; runner_.restore_events(parse_events(stored.events)); }
    return result;
  }

  // ---- the server's options ----
  // A message on the policy topic. Returns false when it is not the contract's. `to_write` is what flash must hold now, or "" when it already does.
  bool on_policy(const std::string& payload, std::string& to_write) {
    to_write.clear();
    if (!runner_.handle_policy(payload)) return false;
    const std::string canonical = policy_text(runner_.policy());
    if (canonical != saved_policy_) to_write = canonical;
    return true;
  }
  void policy_written(const std::string& text) { saved_policy_ = text; }

  // ---- what to write ----
  // Called after the runner's turn. A factory reset (forget) has erased the saved state: nothing is asked again before the restart.
  Writes plan_writes(std::uint64_t now_ms) const {
    Writes out;
    if (forgotten_) return out;
    const std::string now_saved = saved_text(runner_.saved());
    if (now_saved != saved_state_) out.state = now_saved;
    const bool bind = runner_.effective().config_hash;
    out.bind_hash = bind && !out.state.empty();
    out.hash_only = out.state.empty() && bind && !saved_state_.empty() && saved_hash_ != config_hash_;
    const std::string profile_now = runner_.profile() != nullptr && runner_.panel().mode() != Mode::kDisarmed ? profile_json(*runner_.profile()) : std::string();
    if (profile_now != saved_profile_) { out.profile = true; out.profile_text = profile_now; }
    const std::string events_now = events_text(runner_.panel().events());
    const std::vector<Event>& log = runner_.panel().events();
    const bool important = !log.empty() && (log.back().kind == EventKind::kAlarm || log.back().kind == EventKind::kArmed || log.back().kind == EventKind::kDisarmed ||
                                             log.back().kind == EventKind::kLockedOut || log.back().kind == EventKind::kSirenSilenced);
    if (!events_now.empty() && events_now != saved_events_ && (important || now_ms - last_events_write_ms_ >= kEventsWriteEveryMs)) out.events = events_now;
    return out;
  }
  void events_written(const std::string& text, std::uint64_t now_ms) { saved_events_ = text; last_events_write_ms_ = now_ms; }
  void profile_written(const std::string& text) { saved_profile_ = text; }
  void state_written(const std::string& text) { saved_state_ = text; }
  void hash_written() { saved_hash_ = config_hash_; }
  // A factory reset has erased everything the node kept of the panel.
  void forget() { forgotten_ = true; }
  bool forgotten() const { return forgotten_; }

  // ---- what to say ----
  // What changes the message: the panel's phase, mode and siren, the lockout, the profile and the state of each zone.
  std::string signature(std::uint64_t now_ms) const {
    std::string text = std::string(phase_text(runner_.panel().phase())) + mode_text(runner_.panel().mode()) + (runner_.siren() ? "S" : "s") + (runner_.locked_out(now_ms) ? "L" : "l");
    if (runner_.profile() != nullptr) text += "P" + runner_.profile()->id;
    for (const ZoneView& zone : runner_.zones()) if (zone.enabled) text += std::string(state_text(zone.state)) + (zone.bypassed ? "b" : "-");
    return text;
  }
  // The state message when it is due (a change, `force`, or kPublishEveryMs since the last one), or "" when it is not.
  std::string state_message(bool force, std::uint64_t stamp_ms, std::uint64_t now_ms) {
    const std::string now_signature = signature(now_ms);
    if (!force && now_signature == last_signature_ && now_ms - last_publish_ms_ < kPublishEveryMs) return std::string();
    last_signature_ = now_signature;
    last_publish_ms_ = now_ms;
    return runner_.state_json(stamp_ms, now_ms);
  }

 private:
  Runner runner_;
  std::string config_hash_;
  std::string saved_state_, saved_profile_, saved_policy_, saved_hash_, saved_events_;
  std::string last_signature_;
  std::uint64_t last_publish_ms_ = 0, last_events_write_ms_ = 0;
  bool forgotten_ = false;
};

}  // namespace armor::alarm
