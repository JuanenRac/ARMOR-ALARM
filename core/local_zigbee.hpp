// ARMOR-ALARM - the panel speaking Zigbee itself: the coordinator's start-up conversation, the sensors it listens to and the sirens it commands, with no hardware and no network.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// While ARMOR-SERVER's Zigbee2MQTT has the radio the node only lends its serial line (zigbee_bridge.hpp). When that client has been away for a while the node takes the radio over and
// does this: it brings the coordinator up again (SYS_PING, AF_REGISTER, ZDO_STARTUP_FROM_APP), learns which short address each configured sensor and siren has now (a sensor is named by its
// long, IEEE, address in the settings), turns the Zone Status Change Notifications of the sensors into the states of zones, and tells the sirens to sound or to stop. It speaks only
// when the radio is its own. Everything is bytes in, bytes out, and a clock handed in; the firmware owns the UART. Nothing here has talked to a coordinator.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "alarm_zone.hpp"
#include "znp.hpp"
#include "zigbee_ias.hpp"

namespace armor::zigbee {

struct SensorConfig { std::string zone_id; std::uint64_t ieee = 0; };
struct SirenConfig { std::uint64_t ieee = 0; };

// What a sensor said, as the state of its zone.
struct ZoneUpdate {
  std::string zone_id;
  alarm::ZoneState state = alarm::ZoneState::kNormal;
  bool battery_low = false;
};

// A long address as 16 hexadecimal digits (what the settings hold); false for anything else.
inline bool parse_ieee(const std::string& text, std::uint64_t& out) {
  if (text.size() != 16) return false;
  std::uint64_t value = 0;
  for (const char c : text) {
    int digit = -1;
    if (c >= '0' && c <= '9') digit = c - '0'; else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10; else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    if (digit < 0) return false;
    value = (value << 4) | static_cast<std::uint64_t>(digit);
  }
  out = value;
  return true;
}

class LocalZigbee {
 public:
  enum class State { kIdle, kPing, kRegister, kStartup, kWaitNetwork, kReady, kFailed };
  static constexpr std::uint8_t kEndpoint = 1;
  static constexpr std::uint8_t kStateCoordinator = 9;   // ZDO_STATE_CHANGE_IND: the device is up as the network's coordinator
  static constexpr int kTries = 5;
  static constexpr std::uint64_t kStepMs = 3000, kRetryAfterFailureMs = 30'000, kLookupEveryMs = 10'000;

  LocalZigbee(std::vector<SensorConfig> sensors, std::vector<SirenConfig> sirens) {
    for (const SensorConfig& s : sensors) sensors_.push_back(SensorDevice{s, Device{s.ieee, 0}, 0, false});
    for (const SirenConfig& s : sirens) sirens_.push_back(SirenDevice{s.ieee, 0, 0, false});
  }

  // The radio is ours: bring it up. Calling it again starts over.
  void begin(std::uint64_t now_ms) {
    parser_ = znp::Parser();
    tries_ = 0;
    send_step(State::kPing, now_ms);
  }
  // The radio is not ours any more: forget everything about the conversation (the next `begin` starts it afresh).
  void end() { state_ = State::kIdle; out_.clear(); updates_.clear(); }

  State state() const { return state_; }
  bool ready() const { return state_ == State::kReady; }

  // Bytes from the radio.
  void feed(const std::uint8_t* bytes, std::size_t count, std::uint64_t now_ms) {
    if (state_ == State::kIdle) return;
    for (std::size_t i = 0; i < count; ++i) parser_.feed(bytes[i]);
    znp::Frame frame;
    while (parser_.pop(frame)) handle(frame, now_ms);
  }

  // The clock: a step that got no answer is sent again, a coordinator that never answered is tried again after a pause, and the addresses of sensors and sirens not yet known are asked for.
  void tick(std::uint64_t now_ms) {
    switch (state_) {
      case State::kIdle: return;
      case State::kFailed:
        if (now_ms - step_at_ms_ >= kRetryAfterFailureMs) begin(now_ms);
        return;
      case State::kReady: lookups(now_ms); return;
      default:
        if (now_ms - step_at_ms_ >= kStepMs) {
          if (++tries_ >= kTries) { state_ = State::kFailed; step_at_ms_ = now_ms; ++failures_; return; }
          send_current(now_ms);
        }
    }
  }

  // The bytes to write to the radio (and forget).
  std::vector<std::uint8_t> take_output() { std::vector<std::uint8_t> out; out.swap(out_); return out; }
  std::vector<ZoneUpdate> take_updates() { std::vector<ZoneUpdate> out; out.swap(updates_); return out; }

  // The sirens: sound (for `seconds`) or stop. Only a siren whose short address is known is told; the count says how many were.
  std::size_t siren(bool on, std::uint16_t seconds) {
    std::size_t told = 0;
    for (const SirenDevice& s : sirens_) {
      if (s.short_address == 0) continue;
      const std::uint8_t sequence = ++sequence_;
      const std::vector<std::uint8_t> zcl = start_warning(sequence, on ? WarningMode::kBurglar : WarningMode::kStop, SirenLevel::kVeryHigh, on, seconds);
      queue(znp::af_data_request(s.short_address, 1, kEndpoint, kClusterIasWd, sequence, zcl));
      ++told;
    }
    return told;
  }

  // For the panel's page: how the conversation goes.
  std::uint32_t failures() const { return failures_; }
  std::uint32_t reports() const { return reports_; }
  std::uint32_t dropped_frames() const { return parser_.dropped(); }
  std::size_t known_sensors() const { std::size_t n = 0; for (const SensorDevice& s : sensors_) if (s.device.short_address != 0) ++n; return n; }

 private:
  struct Device { std::uint64_t ieee = 0; std::uint16_t short_address = 0; };
  struct SensorDevice { SensorConfig config; Device device; std::uint64_t asked_at_ms = 0; bool asked = false; };
  struct SirenDevice { std::uint64_t ieee = 0; std::uint16_t short_address = 0; std::uint64_t asked_at_ms = 0; bool asked = false; };

  void queue(const znp::Frame& frame) { const std::vector<std::uint8_t> bytes = znp::encode(frame); out_.insert(out_.end(), bytes.begin(), bytes.end()); }

  void send_step(State next, std::uint64_t now_ms) { state_ = next; tries_ = 0; send_current(now_ms); }
  void send_current(std::uint64_t now_ms) {
    step_at_ms_ = now_ms;
    switch (state_) {
      case State::kPing: queue(znp::sys_ping()); break;
      case State::kRegister: queue(znp::af_register(kEndpoint, kProfileHomeAutomation, 0x0400, {}, {kClusterIasZone, kClusterIasWd})); break;
      case State::kStartup: queue(znp::zdo_startup_from_app(100)); break;
      default: break;   // waiting for the network to say it is up: nothing to send
    }
  }

  void handle(const znp::Frame& frame, std::uint64_t now_ms) {
    if (frame.type == znp::Type::kSrsp) {
      if (state_ == State::kPing && frame.is(znp::Type::kSrsp, znp::sub::kSys, 0x01)) { send_step(State::kRegister, now_ms); return; }
      if (state_ == State::kRegister && frame.is(znp::Type::kSrsp, znp::sub::kAf, 0x00)) { send_step(State::kStartup, now_ms); return; }   // success or "already registered": either way it is ours
      if (state_ == State::kStartup && frame.is(znp::Type::kSrsp, znp::sub::kZdo, 0x40)) { state_ = State::kWaitNetwork; step_at_ms_ = now_ms; tries_ = 0; return; }
      return;
    }
    if (frame.type != znp::Type::kAreq) return;
    if (frame.is(znp::Type::kAreq, znp::sub::kZdo, 0xC0) && !frame.data.empty()) {   // ZDO_STATE_CHANGE_IND
      if (frame.data[0] == kStateCoordinator && state_ != State::kIdle && state_ != State::kReady && state_ != State::kPing && state_ != State::kRegister) { state_ = State::kReady; step_at_ms_ = now_ms; }
      return;
    }
    if (frame.is(znp::Type::kAreq, znp::sub::kZdo, 0xC1) && frame.data.size() >= 12) { learn(znp::get16(frame.data, 2), znp::get64(frame.data, 4)); return; }   // END_DEVICE_ANNCE_IND
    if (frame.is(znp::Type::kAreq, znp::sub::kZdo, 0x80) && frame.data.size() >= 11 && frame.data[0] == 0) { learn(znp::get16(frame.data, 9), znp::get64(frame.data, 1)); return; }   // NWK_ADDR_RSP
    if (const std::optional<Incoming> in = incoming_message(frame)) { incoming(*in); return; }
  }

  void learn(std::uint16_t short_address, std::uint64_t ieee) {
    for (SensorDevice& s : sensors_) if (s.config.ieee == ieee) s.device.short_address = short_address;
    for (SirenDevice& s : sirens_) if (s.ieee == ieee) s.short_address = short_address;
  }

  void incoming(const Incoming& in) {
    if (const std::optional<ZoneStatusChange> change = zone_status_change(in)) {
      for (const SensorDevice& s : sensors_) {
        if (s.device.short_address == 0 || s.device.short_address != in.source) continue;
        ++reports_;
        ZoneUpdate update;
        update.zone_id = s.config.zone_id;
        update.state = change->tamper() ? alarm::ZoneState::kTamper : change->triggered() ? alarm::ZoneState::kTriggered : alarm::ZoneState::kNormal;
        update.battery_low = change->battery_low();
        updates_.push_back(update);
      }
      return;
    }
    if (const std::optional<EnrollRequest> enroll = enroll_request(in)) {
      // a sensor that asks to join: answered when it is one of ours (zone ids count from 1), ignored when it is not
      for (std::size_t i = 0; i < sensors_.size(); ++i) {
        if (sensors_[i].device.short_address != in.source) continue;
        const std::uint8_t sequence = ++sequence_;
        queue(znp::af_data_request(in.source, in.source_endpoint, kEndpoint, kClusterIasZone, sequence, zone_enroll_response(sequence, static_cast<std::uint8_t>(i + 1))));
      }
    }
  }

  void lookups(std::uint64_t now_ms) {
    for (SensorDevice& s : sensors_) {
      if (s.device.short_address != 0 || (s.asked && now_ms - s.asked_at_ms < kLookupEveryMs)) continue;
      s.asked = true; s.asked_at_ms = now_ms;
      queue(znp::zdo_nwk_addr_req(s.config.ieee));
    }
    for (SirenDevice& s : sirens_) {
      if (s.short_address != 0 || (s.asked && now_ms - s.asked_at_ms < kLookupEveryMs)) continue;
      s.asked = true; s.asked_at_ms = now_ms;
      queue(znp::zdo_nwk_addr_req(s.ieee));
    }
  }

  State state_ = State::kIdle;
  int tries_ = 0;
  std::uint64_t step_at_ms_ = 0;
  std::uint8_t sequence_ = 0;
  std::uint32_t failures_ = 0, reports_ = 0;
  znp::Parser parser_;
  std::vector<SensorDevice> sensors_;
  std::vector<SirenDevice> sirens_;
  std::vector<std::uint8_t> out_;
  std::vector<ZoneUpdate> updates_;
};

}  // namespace armor::zigbee
