// ARMOR-ALARM - the alarm at work, without the hardware: the wired zones' readings go in, the panel's state and the siren come out.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// What the node does on every turn of its loop: each enabled zone's reading (millivolts at its pin, from the ADC) is classified (alarm_zone.hpp), debounced and handed to the panel
// (alarm_panel.hpp); the panel's clock runs; the siren is on while the panel says so; and the state message is made. The readings and the clock are handed in, so the firmware gives it the
// ADC and the tests give it numbers. A zone that has not been read yet counts as normal, and a zone whose pin cannot be read says nothing at all (it is never taken for a tamper).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "alarm_config.hpp"
#include "alarm_message.hpp"
#include "alarm_panel.hpp"
#include "alarm_zone.hpp"

namespace armor::alarm {

// What the panel's page shows of a zone, beyond what the message carries.
struct ZoneView {
  std::size_t number = 0;     // 1 to 8: its place in the settings
  bool enabled = false;
  std::string id, name;
  ZoneKind kind = ZoneKind::kInstant;
  ZoneState state = ZoneState::kNormal;
  bool bypassed = false;
  bool has_reading = false;
  double millivolts = 0;
  bool digital = false;       // a plain contact on a digital pin: no millivolts, only whether the pin is high
  bool pin_high = false;      // what a digital zone's pin read last
  bool zigbee = false;        // a Zigbee sensor, not a wired loop: it has no millivolts, and a reading only when the node holds the radio
  bool battery_low = false;   // what a Zigbee sensor says of its battery
  bool silent = false;        // a Zigbee sensor that has not been heard for longer than the limit of the settings (see Timing::zigbee_silence_min)
  std::uint64_t heard_ms = 0; // when a Zigbee sensor was last heard
};

// The answer to a command from the server.
struct Reply {
  bool understood = false;   // false: not the contract's command, no answer is sent
  Command command;
  Refusal refusal = Refusal::kNone;
};

class Runner {
 public:
  explicit Runner(const config::Settings& settings)
      : settings_(settings), panel_(settings.panel.timing, zones_of(settings), settings.panel.pin) {
    for (std::size_t i = 0; i < config::kZoneCount; ++i) {
      ZoneView& view = views_[i];
      const config::ZoneSetting& zone = settings.zones[i];
      view.number = i + 1; view.enabled = zone.enabled; view.zigbee = zone.is_zigbee(); view.digital = zone.is_digital(); view.id = zone.id; view.name = zone.name; view.kind = zone.kind;
      filters_[i] = Debouncer(static_cast<std::uint64_t>(zone.hold_ms));
    }
  }

  // The reading of zone `index` (0 to 7) in millivolts at its pin.
  void feed(std::size_t index, double millivolts, std::uint64_t now_ms) {
    if (index >= config::kZoneCount || !views_[index].enabled || views_[index].zigbee || views_[index].digital) return;
    ZoneView& view = views_[index];
    view.has_reading = true;
    view.millivolts = millivolts;
    const ZoneState stable = filters_[index].feed(classify_loop(millivolts, settings_.loop), now_ms);
    view.state = stable;
    panel_.set_zone(view.id, stable, now_ms);
    view.bypassed = panel_.bypassed(view.id);
  }

  // The level of the pin of a digital zone (a contact to ground, pulled up): true when the pin reads high. A zone that is not digital ignores it.
  void feed_digital(std::size_t index, bool pin_high, std::uint64_t now_ms) {
    if (index >= config::kZoneCount || !views_[index].enabled || !views_[index].digital) return;
    ZoneView& view = views_[index];
    const config::ZoneSetting& zone = settings_.zones[index];
    view.has_reading = true;
    view.pin_high = pin_high;
    const ZoneState stable = filters_[index].feed(classify_contact(pin_high, zone.contact != "no", zone.role == "tamper"), now_ms);
    view.state = stable;
    panel_.set_zone(view.id, stable, now_ms);
    view.bypassed = panel_.bypassed(view.id);
  }

  // The level of a button's pin (pressed when it reads low). A press arms the panel once (no PIN: anyone may leave); the answer says whether it did, and why not. A button that is not
  // set up, or is not pressed, answers nothing.
  struct ButtonPress { bool fired = false; Refusal refusal = Refusal::kNone; Mode mode = Mode::kDisarmed; };
  ButtonPress feed_button(std::size_t index, bool pin_high, std::uint64_t now_ms) {
    ButtonPress press;
    if (index >= config::kButtonCount || !settings_.buttons[index].enabled) return press;
    if (!buttons_[index].feed(!pin_high, now_ms)) return press;
    press.fired = true;
    press.mode = settings_.buttons[index].action == "arm_stay" ? Mode::kStay : Mode::kAway;
    press.refusal = panel_.arm(press.mode, false, now_ms);
    return press;
  }

  // What a Zigbee sensor said (the node holds the radio): the state of its zone. An update for a zone that is not a Zigbee one is ignored.
  void feed_zigbee(const std::string& zone_id, ZoneState state, bool battery_low, std::uint64_t now_ms) {
    for (ZoneView& view : views_) {
      if (!view.enabled || !view.zigbee || view.id != zone_id) continue;
      view.has_reading = true;
      view.heard_ms = now_ms;
      view.silent = false;
      view.state = state;
      view.battery_low = battery_low;
      panel_.set_zone(view.id, state, now_ms);
      view.bypassed = panel_.bypassed(view.id);
    }
  }

  void tick(std::uint64_t now_ms) {
    panel_.tick(now_ms);
    supervise_zigbee(now_ms);
    for (ZoneView& view : views_) if (view.enabled) view.bypassed = panel_.bypassed(view.id);
  }

  // From the panel's own page: arming needs no PIN, disarming does.
  Refusal arm(Mode mode, bool force, std::uint64_t now_ms) { return panel_.arm(mode, force, now_ms); }
  Refusal disarm(const std::string& pin, std::uint64_t now_ms) { return panel_.disarm(pin, now_ms); }

  // The state of a Zigbee zone as the server tells it. It counts only while the server holds the radio: when this node holds it, the node hears the sensors itself and what the server says
  // would be older than what it hears. A message that is not the contract's, or for a zone that is not a Zigbee one, changes nothing. True when it was taken.
  bool handle_zone_feed(const std::string& text, std::uint64_t now_ms) {
    ZoneFeed feed;
    if (radio_ || !parse_zone_feed(text, feed)) return false;
    for (const ZoneView& view : views_) {
      if (view.enabled && view.zigbee && view.id == feed.zone_id) {
        feed_zigbee(feed.zone_id, feed.state, feed.battery_low, now_ms);
        return true;
      }
    }
    return false;
  }

  // From the server, as a message. A command that is not the contract's is not understood and gets no answer.
  Reply handle(const std::string& text, std::uint64_t now_ms) {
    Reply reply;
    Command command;
    if (!settings_.server.commands || !parse_command(text, settings_.node_id, command)) return reply;
    reply.understood = true;
    reply.command = command;
    reply.refusal = command.action == Action::kArm ? panel_.arm(command.mode, command.force, now_ms) : panel_.disarm_authorized(now_ms);
    return reply;
  }

  // What a restart must not forget (see Panel::saved): the manager keeps it in flash when it changes and gives it back at start.
  Panel::Saved saved() const { return panel_.saved(); }
  bool restore(const Panel::Saved& saved, std::uint64_t now_ms) {
    const bool armed = panel_.restore(saved, now_ms);
    for (ZoneView& view : views_) if (view.enabled) view.bypassed = panel_.bypassed(view.id);
    return armed;
  }

  // A Zigbee sensor that went quiet is lost (a flat battery, an inhibited radio) and must not keep its last "normal": past the limit it is marked silent and, while the system is armed
  // or arming, its zone says tamper until it is heard again. With the limit at 0 nothing is supervised. A sensor never heard since the start is not judged (nothing to compare with).
  // Whether the radio is this node's: the sensors only reach it while it holds the radio (the server holds it otherwise), so what is not heard then says nothing. Every time the radio is
  // not ours the sensors are given their whole allowance again from the moment it becomes ours.
  void radio_held(bool held, std::uint64_t now_ms) {
    if (!held) for (ZoneView& view : views_) if (view.zigbee) { view.heard_ms = now_ms; view.silent = false; }
    radio_ = held;
  }

  void supervise_zigbee(std::uint64_t now_ms) {
    if (!radio_) return;
    const std::uint64_t limit = static_cast<std::uint64_t>(settings_.panel.timing.zigbee_silence_min) * 60000ULL;
    if (limit == 0) return;
    for (ZoneView& view : views_) {
      if (!view.enabled || !view.zigbee || !view.has_reading) continue;
      const bool quiet = now_ms - view.heard_ms > limit;
      if (quiet == view.silent) { if (quiet && panel_.phase() != Phase::kDisarmed && view.state != ZoneState::kTamper) { view.state = ZoneState::kTamper; panel_.set_zone(view.id, ZoneState::kTamper, now_ms); } continue; }
      view.silent = quiet;
      if (quiet && panel_.phase() != Phase::kDisarmed) { view.state = ZoneState::kTamper; panel_.set_zone(view.id, ZoneState::kTamper, now_ms); }
    }
  }

  void set_epoch(std::int64_t epoch) { panel_.set_epoch(epoch); }
  void restore_events(std::vector<Event> older) { panel_.restore_events(std::move(older)); }
  bool siren() const { return panel_.siren(); }
  const Panel& panel() const { return panel_; }
  const std::array<ZoneView, config::kZoneCount>& zones() const { return views_; }
  bool locked_out(std::uint64_t now_ms) const { return panel_.locked_out(now_ms); }

  std::string state_json(std::uint64_t timestamp_ms, std::uint64_t now_ms) const {
    std::vector<ZoneReport> reports;
    for (const ZoneView& view : views_) if (view.enabled) reports.push_back({view.id, view.name, view.kind, view.state, view.bypassed});
    return alarm::state_json(settings_.node_id, timestamp_ms, now_ms, panel_, panel_.locked_out(now_ms), settings_.server.commands, reports);
  }

  std::string result_json(std::uint64_t timestamp_ms, const Reply& reply) const { return alarm::result_json(settings_.node_id, timestamp_ms, reply.command, reply.refusal, panel_.phase()); }

 private:
  bool radio_ = false;
  static std::vector<Zone> zones_of(const config::Settings& settings) {
    std::vector<Zone> zones;
    for (const config::ZoneSetting& zone : settings.zones) if (zone.enabled) zones.push_back({zone.id, zone.name, zone.kind});
    return zones;
  }

  config::Settings settings_;
  Panel panel_;
  std::array<ZoneView, config::kZoneCount> views_;
  std::array<Debouncer, config::kZoneCount> filters_;
  std::array<ButtonFilter, config::kButtonCount> buttons_;
};

}  // namespace armor::alarm
