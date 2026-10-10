// ARMOR-ALARM - the serial protocol of a Texas Instruments Z-Stack coordinator (ZNP): frames, and the few commands the panel needs. No hardware, no network.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A frame is  SOF (0xFE), LEN (the data's length), CMD0 (the type in its top three bits, the subsystem in the low five), CMD1 (the command), the data, and FCS (the XOR of every byte from
// LEN to the last data byte). The types are SREQ (a request that gets an SRSP), SRSP (its answer) and AREQ (a message nobody asked for). The layouts here are the ones Zigbee2MQTT's
// `zstack` adapter and TI's Z-Stack Monitor and Test API document; they have not been checked against a coordinator.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace armor::znp {

constexpr std::uint8_t kSof = 0xFE;
constexpr std::size_t kMaxData = 250;

enum class Type : std::uint8_t { kPoll = 0, kSreq = 1, kAreq = 2, kSrsp = 3 };
namespace sub { constexpr std::uint8_t kRpcError = 0, kSys = 1, kAf = 4, kZdo = 5, kUtil = 7; }

struct Frame {
  Type type = Type::kSreq;
  std::uint8_t subsystem = 0;
  std::uint8_t command = 0;
  std::vector<std::uint8_t> data;
  bool is(Type t, std::uint8_t s, std::uint8_t c) const { return type == t && subsystem == s && command == c; }
};

inline std::uint8_t checksum(std::uint8_t length, std::uint8_t cmd0, std::uint8_t cmd1, const std::vector<std::uint8_t>& data) {
  std::uint8_t fcs = static_cast<std::uint8_t>(length ^ cmd0 ^ cmd1);
  for (const std::uint8_t byte : data) fcs = static_cast<std::uint8_t>(fcs ^ byte);
  return fcs;
}

// The bytes of a frame; empty when its data is longer than a frame can carry.
inline std::vector<std::uint8_t> encode(const Frame& frame) {
  if (frame.data.size() > kMaxData) return {};
  const std::uint8_t length = static_cast<std::uint8_t>(frame.data.size());
  const std::uint8_t cmd0 = static_cast<std::uint8_t>((static_cast<std::uint8_t>(frame.type) << 5) | (frame.subsystem & 0x1F));
  std::vector<std::uint8_t> out{kSof, length, cmd0, frame.command};
  out.insert(out.end(), frame.data.begin(), frame.data.end());
  out.push_back(checksum(length, cmd0, frame.command, frame.data));
  return out;
}

// Reads frames out of a stream of bytes: whatever comes before a start byte is skipped, and a frame whose checksum is wrong is dropped and counted (the parser then looks for the next
// start byte inside what it had taken as the frame, so a lost byte does not cost the frames after it). Frames wait in a queue until `pop` takes them.
class Parser {
 public:
  void feed(std::uint8_t byte) {
    std::vector<std::uint8_t> again;
    step(byte, again);
    while (!again.empty()) {   // bytes that were taken for part of a bad frame are read once more, from the start byte they may hide
      std::vector<std::uint8_t> bytes;
      bytes.swap(again);
      for (const std::uint8_t b : bytes) step(b, again);
    }
  }
  bool pop(Frame& out) {
    if (ready_.empty()) return false;
    out = std::move(ready_.front());
    ready_.erase(ready_.begin());
    return true;
  }
  std::uint32_t dropped() const { return dropped_; }   // frames thrown away for a wrong checksum or a length that cannot be

 private:
  enum class State { kSof, kLength, kBody };

  // `again` collects the bytes to read once more after a frame that did not check out.
  void step(std::uint8_t byte, std::vector<std::uint8_t>& again) {
    switch (state_) {
      case State::kSof: if (byte == kSof) state_ = State::kLength; buffer_.clear(); return;
      case State::kLength:
        length_ = byte;
        buffer_.clear();
        if (length_ > kMaxData) { ++dropped_; state_ = State::kSof; if (byte == kSof) state_ = State::kLength; return; }
        state_ = State::kBody;
        return;
      case State::kBody:
        buffer_.push_back(byte);
        if (buffer_.size() < static_cast<std::size_t>(length_) + 3) return;   // CMD0, CMD1, the data and the FCS
        finish(again);
        return;
    }
  }

  void finish(std::vector<std::uint8_t>& again) {
    const std::uint8_t cmd0 = buffer_[0], cmd1 = buffer_[1], fcs = buffer_.back();
    std::vector<std::uint8_t> data(buffer_.begin() + 2, buffer_.end() - 1);
    state_ = State::kSof;
    if (checksum(length_, cmd0, cmd1, data) != fcs) {
      ++dropped_;
      for (std::size_t i = 0; i < buffer_.size(); ++i) if (buffer_[i] == kSof) { again.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(i), buffer_.end()); break; }
      buffer_.clear();
      return;
    }
    Frame frame;
    frame.type = static_cast<Type>(cmd0 >> 5);
    frame.subsystem = static_cast<std::uint8_t>(cmd0 & 0x1F);
    frame.command = cmd1;
    frame.data = std::move(data);
    ready_.push_back(std::move(frame));
    buffer_.clear();
  }

  State state_ = State::kSof;
  std::uint8_t length_ = 0;
  std::vector<std::uint8_t> buffer_;
  std::vector<Frame> ready_;
  std::uint32_t dropped_ = 0;
};

// ---- the commands the panel sends --------------------------------------------------------------------------------------------------

inline Frame sreq(std::uint8_t subsystem, std::uint8_t command, std::vector<std::uint8_t> data = {}) { return {Type::kSreq, subsystem, command, std::move(data)}; }

inline void put16(std::vector<std::uint8_t>& out, std::uint16_t value) { out.push_back(static_cast<std::uint8_t>(value & 0xFF)); out.push_back(static_cast<std::uint8_t>(value >> 8)); }
inline void put64(std::vector<std::uint8_t>& out, std::uint64_t value) { for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF)); }
inline std::uint16_t get16(const std::vector<std::uint8_t>& data, std::size_t at) { return static_cast<std::uint16_t>(data[at] | (data[at + 1] << 8)); }
inline std::uint64_t get64(const std::vector<std::uint8_t>& data, std::size_t at) { std::uint64_t v = 0; for (int i = 7; i >= 0; --i) v = (v << 8) | data[at + static_cast<std::size_t>(i)]; return v; }

// SYS_PING: is the coordinator there? Its answer carries the capabilities.
inline Frame sys_ping() { return sreq(sub::kSys, 0x01); }
// AF_REGISTER: the panel's own endpoint (1), on the Home Automation profile, with the IAS Zone and IAS WD clusters as outputs (it receives from sensors and commands sirens).
inline Frame af_register(std::uint8_t endpoint, std::uint16_t profile, std::uint16_t device_id, const std::vector<std::uint16_t>& in_clusters, const std::vector<std::uint16_t>& out_clusters) {
  std::vector<std::uint8_t> d{endpoint};
  put16(d, profile); put16(d, device_id);
  d.push_back(0); d.push_back(0);   // device version, latency
  d.push_back(static_cast<std::uint8_t>(in_clusters.size()));
  for (const std::uint16_t c : in_clusters) put16(d, c);
  d.push_back(static_cast<std::uint8_t>(out_clusters.size()));
  for (const std::uint16_t c : out_clusters) put16(d, c);
  return sreq(sub::kAf, 0x00, std::move(d));
}
// ZDO_STARTUP_FROM_APP: start (or restore) the network after a reset, `delay_ms` from now.
inline Frame zdo_startup_from_app(std::uint16_t delay_ms) { std::vector<std::uint8_t> d; put16(d, delay_ms); return sreq(sub::kZdo, 0x40, std::move(d)); }
// ZDO_NWK_ADDR_REQ: which short address does this long (IEEE) address have now? The answer is an AREQ, ZDO_NWK_ADDR_RSP.
inline Frame zdo_nwk_addr_req(std::uint64_t ieee) { std::vector<std::uint8_t> d; put64(d, ieee); d.push_back(0); d.push_back(0); return sreq(sub::kZdo, 0x00, std::move(d)); }
// AF_DATA_REQUEST: send `payload` (a ZCL frame) from endpoint `src_endpoint` to a device.
inline Frame af_data_request(std::uint16_t destination, std::uint8_t dst_endpoint, std::uint8_t src_endpoint, std::uint16_t cluster, std::uint8_t transaction, const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> d;
  put16(d, destination); d.push_back(dst_endpoint); d.push_back(src_endpoint); put16(d, cluster);
  d.push_back(transaction); d.push_back(0); d.push_back(30);   // options (none), radius
  d.push_back(static_cast<std::uint8_t>(payload.size()));
  d.insert(d.end(), payload.begin(), payload.end());
  return sreq(sub::kAf, 0x01, std::move(d));
}

}  // namespace armor::znp
