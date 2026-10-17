// ARMOR-ALARM - host tests of the Zigbee side of the panel: the ZNP frames, the two alarm clusters, the node's own conversation with a coordinator (against a stand-in that answers as the
// protocol says), who holds the radio, and the settings and the zones that name a sensor. No radio was used.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/alarm_config.hpp"
#include "../core/alarm_runner.hpp"
#include "../core/local_zigbee.hpp"
#include "../core/radio_owner.hpp"
#include "../core/zigbee_ias.hpp"
#include "../core/znp.hpp"

static int failures = 0;
static int checks = 0;
#define CHECK(condition)                                                              \
  do {                                                                                \
    ++checks;                                                                         \
    if (!(condition)) {                                                               \
      ++failures;                                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);                \
    }                                                                                 \
  } while (0)

using namespace armor;
using Bytes = std::vector<std::uint8_t>;

static Bytes frame_bytes(znp::Type type, std::uint8_t subsystem, std::uint8_t command, Bytes data = {}) { return znp::encode(znp::Frame{type, subsystem, command, std::move(data)}); }
static void feed(zigbee::LocalZigbee& z, const Bytes& bytes, std::uint64_t now) { z.feed(bytes.data(), bytes.size(), now); }
static std::vector<znp::Frame> sent_by(zigbee::LocalZigbee& z) {
  const Bytes out = z.take_output();
  znp::Parser p;
  for (const std::uint8_t b : out) p.feed(b);
  std::vector<znp::Frame> frames;
  znp::Frame f;
  while (p.pop(f)) frames.push_back(f);
  return frames;
}

// ---- the frames -----------------------------------------------------------------------------------------------------------------------

static void test_frames() {
  // SYS_PING is FE 00 21 01 20: no data, an SREQ (type 1) of the SYS subsystem (1), command 1, and the checksum of 00 21 01
  CHECK(znp::encode(znp::sys_ping()) == (Bytes{0xFE, 0x00, 0x21, 0x01, 0x20}));
  // the answer carries two bytes of capabilities
  znp::Parser parser;
  const Bytes answer = frame_bytes(znp::Type::kSrsp, znp::sub::kSys, 0x01, {0x79, 0x00});
  CHECK(answer == (Bytes{0xFE, 0x02, 0x61, 0x01, 0x79, 0x00, 0x1B}));
  znp::Frame f;
  for (std::size_t i = 0; i < answer.size(); ++i) { CHECK(!parser.pop(f)); parser.feed(answer[i]); }   // one byte at a time: nothing until the last
  CHECK(parser.pop(f) && f.is(znp::Type::kSrsp, znp::sub::kSys, 0x01) && f.data == (Bytes{0x79, 0x00}) && !parser.pop(f));
  // noise before a frame is skipped, and two frames in one go both come out
  Bytes two{0x00, 0x13, 0x37};
  const Bytes a = frame_bytes(znp::Type::kAreq, znp::sub::kZdo, 0xC0, {0x09}), b = frame_bytes(znp::Type::kSreq, znp::sub::kUtil, 0x00);
  two.insert(two.end(), a.begin(), a.end()); two.insert(two.end(), b.begin(), b.end());
  for (const std::uint8_t byte : two) parser.feed(byte);
  CHECK(parser.pop(f) && f.is(znp::Type::kAreq, znp::sub::kZdo, 0xC0) && parser.pop(f) && f.is(znp::Type::kSreq, znp::sub::kUtil, 0x00) && !parser.pop(f));
  CHECK(parser.dropped() == 0);
  // a wrong checksum is dropped and counted, and the frame hiding behind it is not lost
  Bytes bad = a; bad.back() ^= 0xFF;
  Bytes stream = bad; stream.insert(stream.end(), b.begin(), b.end());
  for (const std::uint8_t byte : stream) parser.feed(byte);
  CHECK(parser.dropped() == 1 && parser.pop(f) && f.is(znp::Type::kSreq, znp::sub::kUtil, 0x00));
  // a lost byte in the middle of one frame must not eat the next
  Bytes cut = a; cut.erase(cut.begin() + 3);
  Bytes stream2 = cut; stream2.insert(stream2.end(), b.begin(), b.end()); stream2.insert(stream2.end(), b.begin(), b.end());
  znp::Parser p2;
  for (const std::uint8_t byte : stream2) p2.feed(byte);
  int good = 0; while (p2.pop(f)) if (f.is(znp::Type::kSreq, znp::sub::kUtil, 0x00)) ++good;
  CHECK(good >= 1 && p2.dropped() >= 1);
  // too long data cannot be encoded
  CHECK(znp::encode(znp::Frame{znp::Type::kSreq, 1, 1, Bytes(251, 0)}).empty());
  // the commands
  const Bytes reg = znp::encode(znp::af_register(1, 0x0104, 0x0400, {}, {0x0500, 0x0502}));
  CHECK(reg[0] == 0xFE && reg[2] == 0x24 && reg[3] == 0x00 && reg[4] == 1 && reg[5] == 0x04 && reg[6] == 0x01 && reg[11] == 0 && reg[12] == 2 && reg[13] == 0x00 && reg[14] == 0x05);
  const Bytes ieee = znp::encode(znp::zdo_nwk_addr_req(0x00158D0001A2B3C4ULL));
  CHECK(ieee[2] == 0x25 && ieee[3] == 0x00 && ieee[4] == 0xC4 && ieee[11] == 0x00 && ieee[12] == 0x00 && ieee[13] == 0x00);
}

// ---- the clusters ---------------------------------------------------------------------------------------------------------------------

static znp::Frame incoming(std::uint16_t cluster, std::uint16_t source, const Bytes& zcl) {
  Bytes d;
  znp::put16(d, 0); znp::put16(d, cluster); znp::put16(d, source);
  d.push_back(1); d.push_back(1); d.push_back(0); d.push_back(200); d.push_back(0);   // endpoints, broadcast, link quality, security
  d.push_back(0); d.push_back(0); d.push_back(0); d.push_back(0);                       // timestamp
  d.push_back(7); d.push_back(static_cast<std::uint8_t>(zcl.size()));
  d.insert(d.end(), zcl.begin(), zcl.end());
  return {znp::Type::kAreq, znp::sub::kAf, 0x81, d};
}

static void test_clusters() {
  // a door contact: Zone Status Change Notification, command 0 from the server of the cluster, status 0x0001 (alarm 1), extended 0, zone id 3, delay 0
  const Bytes open{0x09, 0x42, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0x00};
  const std::optional<zigbee::Incoming> in = zigbee::incoming_message(incoming(0x0500, 0x1234, open));
  CHECK(in && in->cluster == 0x0500 && in->source == 0x1234 && in->source_endpoint == 1 && in->link_quality == 200 && in->payload == open);
  const std::optional<zigbee::ZoneStatusChange> change = zigbee::zone_status_change(*in);
  CHECK(change && change->triggered() && !change->tamper() && !change->battery_low() && change->zone_id == 3);
  // tamper and a low battery
  const Bytes tamper{0x09, 0x43, 0x00, 0x0C, 0x00, 0x00, 0x03, 0x00, 0x00};
  const auto t = zigbee::zone_status_change(*zigbee::incoming_message(incoming(0x0500, 0x1234, tamper)));
  CHECK(t && t->tamper() && t->battery_low() && !t->triggered());
  // closed again
  const Bytes closed{0x09, 0x44, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
  const auto c = zigbee::zone_status_change(*zigbee::incoming_message(incoming(0x0500, 0x1234, closed)));
  CHECK(c && !c->triggered() && !c->tamper());
  // not these: another cluster, a global command (a report of attributes), the client's direction, a short payload, another kind of frame
  CHECK(!zigbee::zone_status_change(*zigbee::incoming_message(incoming(0x0006, 0x1234, open))));
  CHECK(!zigbee::zone_status_change(*zigbee::incoming_message(incoming(0x0500, 0x1234, Bytes{0x08, 0x42, 0x0A, 0x01, 0x00}))));
  CHECK(!zigbee::zone_status_change(*zigbee::incoming_message(incoming(0x0500, 0x1234, Bytes{0x01, 0x42, 0x00, 0x01, 0x00, 0x00, 0x03}))));
  CHECK(!zigbee::zone_status_change(*zigbee::incoming_message(incoming(0x0500, 0x1234, Bytes{0x09, 0x42, 0x00, 0x01}))));
  CHECK(!zigbee::incoming_message(znp::Frame{znp::Type::kAreq, znp::sub::kAf, 0x80, Bytes(30, 0)}));
  CHECK(!zigbee::incoming_message(znp::Frame{znp::Type::kAreq, znp::sub::kAf, 0x81, Bytes(10, 0)}));
  Bytes lying = incoming(0x0500, 0x1234, open).data; lying[16] = 200;   // says more payload than there is
  CHECK(!zigbee::incoming_message(znp::Frame{znp::Type::kAreq, znp::sub::kAf, 0x81, lying}));
  // a manufacturer-specific frame has two more bytes before the sequence number
  const auto m = zigbee::parse_zcl(Bytes{0x0D, 0x34, 0x12, 0x09, 0x00, 0xAA});
  CHECK(m && m->cluster_specific && m->from_server && m->sequence == 9 && m->command == 0 && m->payload == Bytes{0xAA});
  // enrolling
  const auto e = zigbee::enroll_request(*zigbee::incoming_message(incoming(0x0500, 0x1234, Bytes{0x09, 0x10, 0x01, 0x15, 0x00, 0x34, 0x12})));
  CHECK(e && e->zone_type == 0x0015 && e->manufacturer == 0x1234);
  CHECK(zigbee::zone_enroll_response(5, 2) == (Bytes{0x11, 5, 0x00, 0x00, 2}));
  // the siren: burglar mode (1) in the top four bits, strobe on, level very high (3); 60 s
  CHECK(zigbee::start_warning(9, zigbee::WarningMode::kBurglar, zigbee::SirenLevel::kVeryHigh, true, 60) == (Bytes{0x11, 9, 0x00, 0x17, 60, 0, 50, 1}));
  CHECK(zigbee::start_warning(9, zigbee::WarningMode::kStop, zigbee::SirenLevel::kVeryHigh, false, 0)[3] == 0x03);
  // addresses
  std::uint64_t ieee = 0;
  CHECK(zigbee::parse_ieee("00158d0001a2b3c4", ieee) && ieee == 0x00158D0001A2B3C4ULL && zigbee::parse_ieee("00158D0001A2B3C4", ieee));
  CHECK(!zigbee::parse_ieee("00158d0001a2b3c", ieee) && !zigbee::parse_ieee("00158d0001a2b3cz", ieee) && !zigbee::parse_ieee("", ieee) && !zigbee::parse_ieee("0x158d0001a2b3c4", ieee));
}

// ---- the node's conversation with the coordinator --------------------------------------------------------------------------------------

static const std::uint64_t kDoor = 0x00158D0001A2B3C4ULL, kMotion = 0x00158D0001A2B3C5ULL, kSiren = 0x00158D0001FFEE01ULL;

// Plays the coordinator through its start-up: answers each request as the protocol says. Returns the time after.
static std::uint64_t bring_up(zigbee::LocalZigbee& z, std::uint64_t now) {
  z.begin(now);
  auto frames = sent_by(z);
  CHECK(frames.size() == 1 && frames[0].is(znp::Type::kSreq, znp::sub::kSys, 0x01));
  feed(z, frame_bytes(znp::Type::kSrsp, znp::sub::kSys, 0x01, {0x79, 0x00}), now += 20);
  frames = sent_by(z);
  CHECK(frames.size() == 1 && frames[0].is(znp::Type::kSreq, znp::sub::kAf, 0x00) && frames[0].data[0] == 1);
  feed(z, frame_bytes(znp::Type::kSrsp, znp::sub::kAf, 0x00, {0x00}), now += 20);
  frames = sent_by(z);
  CHECK(frames.size() == 1 && frames[0].is(znp::Type::kSreq, znp::sub::kZdo, 0x40));
  CHECK(!z.ready());
  feed(z, frame_bytes(znp::Type::kSrsp, znp::sub::kZdo, 0x40, {0x00}), now += 20);   // 0: the network was restored
  CHECK(!z.ready());
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kZdo, 0xC0, {0x09}), now += 300);  // the device is up as the coordinator
  CHECK(z.ready());
  return now;
}

static void test_conversation() {
  zigbee::LocalZigbee z({{"front-door", kDoor}, {"hall-motion", kMotion}}, {{kSiren}});
  CHECK(z.state() == zigbee::LocalZigbee::State::kIdle);
  z.tick(1000);
  CHECK(z.take_output().empty());                                    // idle says nothing
  std::uint64_t now = bring_up(z, 1000);
  // ready: it asks for the short address of every sensor and siren it does not know
  z.tick(now + 1);
  auto asks = sent_by(z);
  CHECK(asks.size() == 3 && asks[0].is(znp::Type::kSreq, znp::sub::kZdo, 0x00) && znp::get64(asks[0].data, 0) == kDoor && znp::get64(asks[1].data, 0) == kMotion && znp::get64(asks[2].data, 0) == kSiren);
  z.tick(now + 2000);
  CHECK(z.take_output().empty());                                    // not again at once
  // the answers: NWK_ADDR_RSP (status, IEEE, short address, ...)
  const auto rsp = [&](std::uint64_t ieee, std::uint16_t addr) { Bytes d{0}; znp::put64(d, ieee); znp::put16(d, addr); d.push_back(0); d.push_back(0); return frame_bytes(znp::Type::kAreq, znp::sub::kZdo, 0x80, d); };
  feed(z, rsp(kDoor, 0x1234), now += 100);
  feed(z, rsp(kMotion, 0x2345), now);
  CHECK(z.known_sensors() == 2);
  // a sensor reports: the door opens, then closes; a device that is not ours is ignored
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x1234, Bytes{0x09, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00}).data), now += 50);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x7777, Bytes{0x09, 0x02, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00}).data), now += 50);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x1234, Bytes{0x09, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00}).data), now += 50);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x2345, Bytes{0x09, 0x04, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0x00}).data), now += 50);
  const std::vector<zigbee::ZoneUpdate> updates = z.take_updates();
  CHECK(updates.size() == 3 && updates[0].zone_id == "front-door" && updates[0].state == alarm::ZoneState::kTriggered && updates[1].state == alarm::ZoneState::kNormal
        && updates[2].zone_id == "hall-motion" && updates[2].state == alarm::ZoneState::kTamper && z.reports() == 3 && z.take_updates().empty());
  // an announce (a device rejoined with a new short address) is learned
  Bytes announce; znp::put16(announce, 0); znp::put16(announce, 0x9999); znp::put64(announce, kDoor); announce.push_back(0x80);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kZdo, 0xC1, announce), now += 10);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x9999, Bytes{0x09, 0x05, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00}).data), now += 10);
  CHECK(z.take_updates().size() == 1);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x1234, Bytes{0x09, 0x06, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00}).data), now += 10);
  CHECK(z.take_updates().empty());                                   // the old address is nobody's now
  // the siren: unknown address, so nobody is told; after its answer, told to sound and to stop
  CHECK(z.siren(true, 60) == 0 && z.take_output().empty());
  feed(z, rsp(kSiren, 0x3456), now += 10);
  CHECK(z.siren(true, 60) == 1);
  auto sound = sent_by(z);
  CHECK(sound.size() == 1 && sound[0].is(znp::Type::kSreq, znp::sub::kAf, 0x01) && znp::get16(sound[0].data, 0) == 0x3456 && znp::get16(sound[0].data, 4) == 0x0502 && sound[0].data[9] == 8 && sound[0].data[10] == 0x11 && sound[0].data[12] == 0x00 && sound[0].data[13] == 0x17);
  CHECK(z.siren(false, 0) == 1);
  sound = sent_by(z);
  CHECK(sound.size() == 1 && sound[0].data[13] == 0x03);
  // a sensor that asks to join is answered, a stranger is not
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x2345, Bytes{0x09, 0x10, 0x01, 0x0D, 0x00, 0x34, 0x12}).data), now += 10);
  auto reply = sent_by(z);
  CHECK(reply.size() == 1 && znp::get16(reply[0].data, 0) == 0x2345 && reply[0].data[10] == 0x11 && reply[0].data[14] == 2);
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kAf, 0x81, incoming(0x0500, 0x7777, Bytes{0x09, 0x10, 0x01, 0x0D, 0x00, 0x34, 0x12}).data), now += 10);
  CHECK(sent_by(z).empty());
  // when the radio is not ours any more it says and does nothing
  z.end();
  feed(z, frame_bytes(znp::Type::kAreq, znp::sub::kZdo, 0xC0, {0x09}), now += 10);
  CHECK(z.state() == zigbee::LocalZigbee::State::kIdle && z.take_output().empty() && z.take_updates().empty());
}

static void test_a_coordinator_that_does_not_answer() {
  zigbee::LocalZigbee z({}, {});
  z.begin(0);
  sent_by(z);
  // sent again every few seconds, five times in all, then given up, and tried afresh after a pause
  int sends = 1;
  std::uint64_t now = 0;
  while (z.state() == zigbee::LocalZigbee::State::kPing && now < 60'000) { now += 100; z.tick(now); if (!z.take_output().empty()) ++sends; }
  CHECK(z.state() == zigbee::LocalZigbee::State::kFailed && sends == zigbee::LocalZigbee::kTries && z.failures() == 1);
  z.tick(now + 1000);
  CHECK(z.state() == zigbee::LocalZigbee::State::kFailed && z.take_output().empty());
  z.tick(now + zigbee::LocalZigbee::kRetryAfterFailureMs);
  CHECK(z.state() == zigbee::LocalZigbee::State::kPing && sent_by(z).size() == 1);
  // an answer to the wrong request moves nothing
  feed(z, frame_bytes(znp::Type::kSrsp, znp::sub::kZdo, 0x40, {0x00}), now + 40'000);
  CHECK(z.state() == zigbee::LocalZigbee::State::kPing);
  // a network that says it is up before the start-up was asked for is not taken for the real thing: only after the start-up request
  zigbee::LocalZigbee y({}, {});
  y.begin(0);
  feed(y, frame_bytes(znp::Type::kAreq, znp::sub::kZdo, 0xC0, {0x09}), 10);
  CHECK(!y.ready());
  // garbage on the line does not break it
  Bytes noise(300);
  for (std::size_t i = 0; i < noise.size(); ++i) noise[i] = static_cast<std::uint8_t>(i * 37 + 11);
  feed(y, noise, 20);
  feed(y, frame_bytes(znp::Type::kSrsp, znp::sub::kSys, 0x01, {0x79, 0x00}), 30);
  CHECK(y.state() == zigbee::LocalZigbee::State::kRegister);
}

// ---- who holds the radio ----------------------------------------------------------------------------------------------------------------

static void test_owner() {
  zigbee::RadioOwner owner(30'000, 0);
  using O = zigbee::RadioOwner::Owner;
  CHECK(owner.update(false, 1000) == O::kClient && owner.update(false, 29'999) == O::kClient);   // a node that has just started waits for the server like any other absence
  CHECK(owner.update(false, 30'000) == O::kNode && owner.takeovers() == 1 && owner.update(false, 90'000) == O::kNode && owner.takeovers() == 1);
  CHECK(owner.update(true, 91'000) == O::kClient);                       // the server is back: at once
  CHECK(owner.update(false, 92'000) == O::kClient && owner.update(false, 120'999) == O::kClient && owner.update(false, 121'000) == O::kNode && owner.takeovers() == 2);
  zigbee::RadioOwner never(0);
  CHECK(never.update(false, 10'000'000) == O::kClient && never.takeovers() == 0);
}

// ---- the settings and the zones ----------------------------------------------------------------------------------------------------------

static bool has(const config::Problems& problems, const std::string& path, const std::string& code) {
  for (const config::Problem& p : problems) if (p.path == path && p.code == code) return true;
  return false;
}

static config::Settings valid_settings() {
  config::Settings s = config::default_settings("a1b2c3");
  s.sta.enabled = true; s.sta.ssid = "casa"; s.sta.password = "una-clave-larga";
  return s;
}

static void test_settings() {
  config::Settings s = valid_settings();
  CHECK(s.zigbee_local.after_s == 0 && !s.zones[0].is_zigbee());
  s.zones[0].enabled = true; s.zones[0].id = "front-door"; s.zones[0].zigbee = "00158d0001a2b3c4"; s.zones[0].pin = -1;
  s.panel.pin = "1234";
  CHECK(has(config::validate(s), "zones.0.zigbee", "needs_local"));
  s.zigbee_local.after_s = 60;
  CHECK(has(config::validate(s), "zigbee_local.after_s", "needs_zigbee"));
  s.zigbee.enabled = true; s.zigbee.allowed_client = "192.168.0.180"; s.zigbee.rx = board::kZigbeePins.rx; s.zigbee.tx = board::kZigbeePins.tx;
  CHECK(config::validate(s).empty());                                    // a Zigbee zone needs no analog pin
  s.zigbee_local.after_s = 5;
  CHECK(has(config::validate(s), "zigbee_local.after_s", "range"));
  s.zigbee_local.after_s = 60;
  s.zones[1].enabled = true; s.zones[1].id = "motion"; s.zones[1].zigbee = "00158d0001a2b3c4";
  CHECK(has(config::validate(s), "zones.1.zigbee", "conflict"));
  s.zones[1].zigbee = "not-an-address";
  CHECK(has(config::validate(s), "zones.1.zigbee", "invalid"));
  s.zones[1].zigbee = "00158d0001a2b3c5";
  s.zigbee_local.sirens[0] = "00158d0001ffee01";
  CHECK(config::validate(s).empty());
  s.zigbee_local.sirens[1] = "nope";
  CHECK(has(config::validate(s), "zigbee_local.sirens.1", "invalid"));
  s.zigbee_local.sirens[1].clear();
  // written and read back, the way the flash keeps it
  const std::string stored = config::to_json(s, true);
  config::Settings back;
  config::Problems problems;
  CHECK(config::load(stored, config::default_settings("a1b2c3"), back, problems) && back.zones[0].zigbee == "00158d0001a2b3c4" && back.zigbee_local.after_s == 60 && back.zigbee_local.sirens[0] == "00158d0001ffee01" && back.zigbee_local.sirens[1].empty());
  problems.clear();
  CHECK(!config::load("{\"zigbee_local\":{\"sirens\":\"all\"}}", s, back, problems) && has(problems, "zigbee_local.sirens", "invalid"));
}

static void test_runner() {
  config::Settings s = valid_settings();
  s.zigbee.enabled = true; s.zigbee_local.after_s = 60;
  s.zones[0].enabled = true; s.zones[0].id = "front-door"; s.zones[0].kind = alarm::ZoneKind::kEntry; s.zones[0].zigbee = "00158d0001a2b3c4"; s.zones[0].pin = -1;
  s.zones[1].enabled = true; s.zones[1].id = "window"; s.zones[1].kind = alarm::ZoneKind::kInstant;
  s.panel.pin = "1234"; s.panel.timing.exit_delay_s = 5; s.panel.timing.entry_delay_s = 5; s.panel.timing.siren_max_s = 60;
  alarm::Runner node(s);
  std::uint64_t now = 0;
  node.tick(now);
  CHECK(node.zones()[0].zigbee && !node.zones()[0].has_reading && !node.zones()[1].zigbee);
  node.feed(0, 1800, now);                                               // a reading of a pin for a Zigbee zone means nothing
  CHECK(!node.zones()[0].has_reading && node.zones()[0].state == alarm::ZoneState::kNormal);
  for (int i = 0; i < 20; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
  CHECK(node.arm(alarm::Mode::kAway, false, now) == alarm::Refusal::kNone);
  for (int i = 0; i < 60; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
  CHECK(node.panel().phase() == alarm::Phase::kArmed);
  node.feed_zigbee("front-door", alarm::ZoneState::kTriggered, true, now);
  CHECK(node.zones()[0].has_reading && node.zones()[0].state == alarm::ZoneState::kTriggered && node.zones()[0].battery_low);
  now += 100; node.tick(now);
  CHECK(node.panel().phase() == alarm::Phase::kEntryDelay);              // an entry zone starts the entry delay, from a Zigbee sensor as from a loop
  node.feed_zigbee("nothing", alarm::ZoneState::kTriggered, false, now);   // an unknown zone, or a wired one, changes nothing
  node.feed_zigbee("window", alarm::ZoneState::kTriggered, false, now);
  CHECK(node.zones()[1].state == alarm::ZoneState::kNormal);
}

static void test_network_coordinator_setting() {
  config::Settings s = valid_settings();
  s.zones[0].enabled = true; s.zones[0].id = "front-door"; s.zones[0].zigbee = "00158d0001a2b3c4"; s.zones[0].pin = -1;
  s.panel.pin = "1234";
  s.zigbee_local.after_s = 60;
  // a coordinator on the network stands for the radio on the board: the local settings need one of them
  CHECK(has(config::validate(s), "zigbee_local.after_s", "needs_zigbee"));
  s.zigbee.network_host = "192.168.0.201";
  CHECK(config::validate(s).empty());
  // written and read back, the way the flash keeps it
  s.zigbee.network_port = 7000;
  const std::string stored = config::to_json(s, true);
  config::Settings back;
  config::Problems problems;
  CHECK(config::load(stored, config::default_settings("a1b2c3"), back, problems) && back.zigbee.network_host == "192.168.0.201" && back.zigbee.network_port == 7000);
  // a bad address is refused, and a coordinator next to a radio on the board is a conflict
  s.zigbee.network_host = "not-an-address";
  CHECK(has(config::validate(s), "zigbee.network_host", "invalid"));
  s.zigbee.network_host = "192.168.0.201";
  s.zigbee.enabled = true; s.zigbee.allowed_client = "192.168.0.180"; s.zigbee.rx = board::kZigbeePins.rx; s.zigbee.tx = board::kZigbeePins.tx;
  CHECK(has(config::validate(s), "zigbee.network_host", "conflict"));
  problems.clear();
  CHECK(!config::load("{\"zigbee\":{\"network_host\":\"192.168.0.201\",\"network_port\":70000}}", s, back, problems) && has(problems, "zigbee.network_port", "range"));
}

static void test_server_feed() {
  config::Settings s = valid_settings();
  s.zigbee.enabled = true;
  s.zones[0].enabled = true; s.zones[0].id = "front-door"; s.zones[0].kind = alarm::ZoneKind::kInstant; s.zones[0].zigbee = "00158d0001a2b3c4"; s.zones[0].pin = -1;
  s.zones[1].enabled = true; s.zones[1].id = "window"; s.zones[1].kind = alarm::ZoneKind::kInstant;
  s.panel.pin = "1234"; s.panel.timing.exit_delay_s = 1; s.panel.timing.entry_delay_s = 5; s.panel.timing.siren_max_s = 60;
  // the message is strict
  alarm::ZoneFeed feed;
  CHECK(alarm::parse_zone_feed("{\"zone\":\"front-door\",\"state\":\"triggered\",\"battery_low\":true}", feed) && feed.zone_id == "front-door" && feed.state == alarm::ZoneState::kTriggered && feed.battery_low);
  CHECK(alarm::parse_zone_feed("{\"zone\":\"a\",\"state\":\"tamper\"}", feed) && feed.state == alarm::ZoneState::kTamper && !feed.battery_low);
  CHECK(!alarm::parse_zone_feed("{\"zone\":\"a\",\"state\":\"open\"}", feed));                         // not one of the three words
  CHECK(!alarm::parse_zone_feed("{\"zone\":\"a\",\"state\":\"normal\",\"extra\":1}", feed));              // an unknown field
  CHECK(!alarm::parse_zone_feed("{\"state\":\"normal\"}", feed) && !alarm::parse_zone_feed("[]", feed) && !alarm::parse_zone_feed("nope", feed));
  // while the server holds the radio (the node does not), the zone follows what the server says and arming the panel protects it
  alarm::Runner node(s);
  std::uint64_t now = 0;
  node.tick(now);
  CHECK(node.handle_zone_feed("{\"zone\":\"front-door\",\"state\":\"normal\",\"battery_low\":false}", now) && node.zones()[0].has_reading);
  for (int i = 0; i < 20; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
  CHECK(node.arm(alarm::Mode::kAway, false, now) == alarm::Refusal::kNone);
  for (int i = 0; i < 30; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
  CHECK(node.panel().phase() == alarm::Phase::kArmed);
  CHECK(node.handle_zone_feed("{\"zone\":\"front-door\",\"state\":\"triggered\",\"battery_low\":true}", now));
  CHECK(node.zones()[0].state == alarm::ZoneState::kTriggered && node.zones()[0].battery_low);
  now += 100; node.tick(now);
  CHECK(node.panel().phase() == alarm::Phase::kAlarm);                                                 // an instant Zigbee zone opened with the system armed: the panel alarms by itself
  // a wired zone, an unknown one or a bad message changes nothing
  CHECK(!node.handle_zone_feed("{\"zone\":\"window\",\"state\":\"triggered\"}", now) && node.zones()[1].state == alarm::ZoneState::kNormal);
  CHECK(!node.handle_zone_feed("{\"zone\":\"nothing\",\"state\":\"triggered\"}", now) && !node.handle_zone_feed("garbage", now));
  // when this node holds the radio it hears the sensors itself: the server's word is not taken
  alarm::Runner held(s);
  held.radio_held(true, 0);
  CHECK(!held.handle_zone_feed("{\"zone\":\"front-door\",\"state\":\"triggered\"}", 0) && !held.zones()[0].has_reading);
}

static void test_silent_sensor() {
  config::Settings s = valid_settings();
  s.zigbee.enabled = true;
  s.zones[0].enabled = true; s.zones[0].id = "front-door"; s.zones[0].kind = alarm::ZoneKind::kInstant; s.zones[0].zigbee = "00158d0001a2b3c4"; s.zones[0].pin = -1;
  s.panel.pin = "1234"; s.panel.timing.exit_delay_s = 1; s.panel.timing.entry_delay_s = 5; s.panel.timing.siren_max_s = 60; s.panel.timing.zigbee_silence_min = 10;
  alarm::Runner node(s);
  std::uint64_t now = 0;
  node.radio_held(true, now);
  node.feed_zigbee("front-door", alarm::ZoneState::kNormal, false, now);
  CHECK(node.arm(alarm::Mode::kAway, false, now) == alarm::Refusal::kNone);
  for (int i = 0; i < 30; ++i) { now += 100; node.tick(now); }
  CHECK(node.panel().phase() == alarm::Phase::kArmed);
  now += 9 * 60 * 1000; node.tick(now);
  CHECK(!node.zones()[0].silent && node.panel().phase() == alarm::Phase::kArmed);          // not yet
  now += 2 * 60 * 1000; node.tick(now);
  CHECK(node.zones()[0].silent && node.zones()[0].state == alarm::ZoneState::kTamper);      // lost: while armed it is a tamper
  CHECK(node.panel().phase() == alarm::Phase::kAlarm);
  // hearing it again clears the silence
  node.feed_zigbee("front-door", alarm::ZoneState::kNormal, false, now);
  CHECK(!node.zones()[0].silent && node.zones()[0].state == alarm::ZoneState::kNormal);
  // when the radio is the server's the sensors are judged by its feed; the allowance starts again whenever the radio changes hands
  alarm::Runner other(s);
  other.radio_held(false, 0);
  other.feed_zigbee("front-door", alarm::ZoneState::kNormal, false, 0);
  other.tick(5 * 60 * 1000);
  CHECK(!other.zones()[0].silent);
  other.tick(11 * 60 * 1000);
  CHECK(other.zones()[0].silent);                                                           // the feed stopped: lost
  other.handle_zone_feed("{\"zone\":\"front-door\",\"state\":\"normal\"}", 11 * 60 * 1000);
  other.radio_held(true, 100 * 60 * 1000);
  other.tick(100 * 60 * 1000 + 5 * 60 * 1000);
  CHECK(!other.zones()[0].silent);
  other.tick(100 * 60 * 1000 + 11 * 60 * 1000);
  CHECK(other.zones()[0].silent);
  // a sensor that went silent with the system disarmed is looked at when arming: refused, or left out when forced, and it does not raise the alarm in the exit delay
  alarm::Runner lost(s);
  lost.radio_held(true, 0);
  lost.feed_zigbee("front-door", alarm::ZoneState::kNormal, false, 0);
  lost.tick(11 * 60 * 1000);
  CHECK(lost.zones()[0].silent && lost.panel().phase() == alarm::Phase::kDisarmed);
  CHECK(lost.arm(alarm::Mode::kAway, false, 11 * 60 * 1000) == alarm::Refusal::kZonesOpen);
  CHECK(lost.arm(alarm::Mode::kAway, true, 11 * 60 * 1000) == alarm::Refusal::kNone);
  for (int i = 1; i <= 30; ++i) lost.tick(11 * 60 * 1000 + i * 100);
  CHECK(lost.panel().phase() == alarm::Phase::kArmed && !lost.siren());
  CHECK(lost.disarm("1234", 11 * 60 * 1000 + 4000) == alarm::Refusal::kNone);
  // a sensor never heard is judged from the moment the node began to listen, and armed it is a tamper
  alarm::Runner mute(s);
  mute.radio_held(true, 0);
  CHECK(mute.arm(alarm::Mode::kAway, false, 0) == alarm::Refusal::kNone);
  mute.tick(2000);
  CHECK(mute.panel().phase() == alarm::Phase::kArmed && !mute.zones()[0].silent);
  mute.tick(11 * 60 * 1000);
  CHECK(mute.zones()[0].silent && mute.panel().phase() == alarm::Phase::kAlarm);
}

int main() {
  test_frames();
  test_clusters();
  test_conversation();
  test_a_coordinator_that_does_not_answer();
  test_owner();
  test_settings();
  test_runner();
  test_server_feed();
  test_network_coordinator_setting();
  test_silent_sensor();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
