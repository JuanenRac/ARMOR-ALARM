// ARMOR-ALARM - the two Zigbee clusters of an alarm, read and written as ZNP frames: IAS Zone (what a door contact, a motion sensor or a smoke detector says) and IAS WD (the siren).
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// A sensor reports with a *Zone Status Change Notification*: a 16-bit status whose bits say alarm, tamper, low battery and so on. The coordinator hands it over as an AF_INCOMING_MSG
// whose payload is a ZCL frame. A siren is told to sound with a *Start Warning* command of the IAS WD cluster inside an AF_DATA_REQUEST. The layouts are the Zigbee Cluster Library's and
// Z-Stack's; nothing here has been checked against a real sensor.
#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include "znp.hpp"

namespace armor::zigbee {

constexpr std::uint16_t kClusterIasZone = 0x0500, kClusterIasWd = 0x0502;
constexpr std::uint16_t kProfileHomeAutomation = 0x0104;

// What AF_INCOMING_MSG carries (the fields the panel uses).
struct Incoming {
  std::uint16_t cluster = 0;
  std::uint16_t source = 0;
  std::uint8_t source_endpoint = 0;
  std::uint8_t link_quality = 0;
  std::vector<std::uint8_t> payload;   // the ZCL frame
};

// Reads an AF_INCOMING_MSG (an AREQ of the AF subsystem, command 0x81). Nothing for any other frame or for a short one.
inline std::optional<Incoming> incoming_message(const znp::Frame& frame) {
  if (!frame.is(znp::Type::kAreq, znp::sub::kAf, 0x81) || frame.data.size() < 17) return std::nullopt;
  const std::vector<std::uint8_t>& d = frame.data;
  // GroupId(2) ClusterId(2) SrcAddr(2) SrcEndpoint(1) DstEndpoint(1) WasBroadcast(1) LinkQuality(1) SecurityUse(1) Timestamp(4) TransSeq(1) Len(1) Data
  const std::size_t length = d[16];
  if (d.size() < 17 + length) return std::nullopt;
  Incoming in;
  in.cluster = znp::get16(d, 2);
  in.source = znp::get16(d, 4);
  in.source_endpoint = d[6];
  in.link_quality = d[9];
  in.payload.assign(d.begin() + 17, d.begin() + 17 + static_cast<std::ptrdiff_t>(length));
  return in;
}

// A ZCL frame: a cluster-specific command (not a global one such as a report of attributes), with its sequence number, command id and payload.
struct Zcl {
  bool cluster_specific = false;
  bool from_server = false;      // direction: sent by the cluster's server (a sensor), not by its client
  std::uint8_t sequence = 0;
  std::uint8_t command = 0;
  std::vector<std::uint8_t> payload;
};

inline std::optional<Zcl> parse_zcl(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 3) return std::nullopt;
  const std::uint8_t control = bytes[0];
  std::size_t at = 1;
  if ((control & 0x04) != 0) at += 2;   // a manufacturer code
  if (bytes.size() < at + 2) return std::nullopt;
  Zcl z;
  z.cluster_specific = (control & 0x03) == 0x01;
  z.from_server = (control & 0x08) != 0;
  z.sequence = bytes[at];
  z.command = bytes[at + 1];
  z.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at) + 2, bytes.end());
  return z;
}

// ---- IAS Zone ---------------------------------------------------------------------------------------------------------------------

namespace zone_status {
constexpr std::uint16_t kAlarm1 = 0x0001, kAlarm2 = 0x0002, kTamper = 0x0004, kBatteryLow = 0x0008, kSupervision = 0x0010, kRestore = 0x0020, kTrouble = 0x0040, kAcFault = 0x0080;
}

struct ZoneStatusChange {
  std::uint16_t status = 0;
  std::uint8_t zone_id = 0;
  bool triggered() const { return (status & (zone_status::kAlarm1 | zone_status::kAlarm2)) != 0; }
  bool tamper() const { return (status & zone_status::kTamper) != 0; }
  bool battery_low() const { return (status & zone_status::kBatteryLow) != 0; }
  bool trouble() const { return (status & (zone_status::kTrouble | zone_status::kAcFault)) != 0; }
};

// Zone Status Change Notification: command 0x00 of the cluster, sent by the sensor: ZoneStatus(2), ExtendedStatus(1), ZoneId(1), Delay(2).
inline std::optional<ZoneStatusChange> zone_status_change(const Incoming& in) {
  if (in.cluster != kClusterIasZone) return std::nullopt;
  const std::optional<Zcl> z = parse_zcl(in.payload);
  if (!z || !z->cluster_specific || !z->from_server || z->command != 0x00 || z->payload.size() < 4) return std::nullopt;
  ZoneStatusChange change;
  change.status = znp::get16(z->payload, 0);
  change.zone_id = z->payload[3];
  return change;
}

// Zone Enroll Request (command 0x01, from the sensor): a sensor that asks to join the alarm. The answer is a Zone Enroll Response with this zone id.
struct EnrollRequest { std::uint16_t zone_type = 0, manufacturer = 0; };
inline std::optional<EnrollRequest> enroll_request(const Incoming& in) {
  if (in.cluster != kClusterIasZone) return std::nullopt;
  const std::optional<Zcl> z = parse_zcl(in.payload);
  if (!z || !z->cluster_specific || !z->from_server || z->command != 0x01 || z->payload.size() < 4) return std::nullopt;
  return EnrollRequest{znp::get16(z->payload, 0), znp::get16(z->payload, 2)};
}

// Zone Enroll Response (command 0x00 of the client): code 0 is success, then the zone id the panel gave it. A cluster-specific command from the client to the server.
inline std::vector<std::uint8_t> zone_enroll_response(std::uint8_t sequence, std::uint8_t zone_id) { return {0x11, sequence, 0x00, 0x00, zone_id}; }

// ---- IAS WD (the siren) -------------------------------------------------------------------------------------------------------------

enum class WarningMode : std::uint8_t { kStop = 0, kBurglar = 1, kFire = 2, kEmergency = 3 };
enum class SirenLevel : std::uint8_t { kLow = 0, kMedium = 1, kHigh = 2, kVeryHigh = 3 };

// Start Warning (command 0x00 of the cluster, sent by the panel): warning info (mode in bits 4 to 7, strobe in bit 2, siren level in bits 0 and 1), duration in seconds (16 bits), strobe
// duty cycle (percent) and strobe level. A warning with mode `kStop` silences the siren.
inline std::vector<std::uint8_t> start_warning(std::uint8_t sequence, WarningMode mode, SirenLevel level, bool strobe, std::uint16_t seconds) {
  const std::uint8_t info = static_cast<std::uint8_t>((static_cast<std::uint8_t>(mode) << 4) | (strobe ? 0x04 : 0x00) | static_cast<std::uint8_t>(level));
  return {0x11, sequence, 0x00, info, static_cast<std::uint8_t>(seconds & 0xFF), static_cast<std::uint8_t>(seconds >> 8), 50, 1};
}

}  // namespace armor::zigbee
