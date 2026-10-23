// ARMOR-ALARM - host tests of the security options the server keeps for the panel: SHA-256 and HMAC against known values, the hash that ties the saved armed state to the zones and the PIN, the
// server's policy message, and the checks of the signed zone feed (signature, sequence number, the options on the node). No hardware was used.
#include <cstdio>
#include <string>

#include "../core/alarm_config.hpp"
#include "../core/alarm_policy.hpp"
#include "../core/alarm_runner.hpp"
#include "../core/hmac_sha256.hpp"

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

static const char* kKey = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
// The value ARMOR-COMMON's zone_feed_signature and ARMOR-SERVER give for key kKey, node "alarm-1", zone "z1", triggered, battery_low false, seq 1700000000000.
static const char* kSignature = "330aa293a19d730d0deede3598b3efbed43887483298f15832c9ba4bfbda0378";

static config::Settings panel_settings() {
  config::Settings s = config::default_settings("a1b2c3");
  s.sta.enabled = true; s.sta.ssid = "casa"; s.sta.password = "una-clave-larga";
  s.node_id = "alarm-1";
  s.zigbee.enabled = true; s.zigbee.allowed_client = "192.168.0.180"; s.zigbee.rx = board::kZigbeePins.rx; s.zigbee.tx = board::kZigbeePins.tx; s.zigbee_local.after_s = 60;
  s.zones[0].enabled = true; s.zones[0].id = "z1"; s.zones[0].kind = alarm::ZoneKind::kInstant; s.zones[0].zigbee = "00158d0001a2b3c4"; s.zones[0].pin = -1;
  s.zones[1].enabled = true; s.zones[1].id = "window"; s.zones[1].kind = alarm::ZoneKind::kInstant;
  s.panel.pin = "1234"; s.panel.timing.exit_delay_s = 1; s.panel.timing.entry_delay_s = 5; s.panel.timing.siren_max_s = 60;
  return s;
}

static std::string signed_feed(const std::string& state, std::uint64_t seq, const std::string& key = kKey, const std::string& node = "alarm-1") {
  alarm::ZoneFeed feed;
  feed.zone_id = "z1";
  feed.state = state == "triggered" ? alarm::ZoneState::kTriggered : state == "tamper" ? alarm::ZoneState::kTamper : alarm::ZoneState::kNormal;
  feed.seq = seq;
  const std::string sig = alarm::zone_feed_signature(key, node, feed);
  return "{\"zone\":\"z1\",\"state\":\"" + state + "\",\"battery_low\":false,\"seq\":" + std::to_string(seq) + ",\"sig\":\"" + sig + "\"}";
}

static void test_hashes_against_known_values() {
  CHECK(hash::to_hex(hash::sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(hash::to_hex(hash::sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(hash::to_hex(hash::sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");   // two blocks
  const std::string million(1000, 'a');
  hash::Sha256 long_one;
  for (int i = 0; i < 1000; ++i) long_one.update(million);
  CHECK(hash::to_hex(long_one.finish()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");   // a million a's, fed in pieces
  const std::string key(20, '\x0b');   // RFC 4231, test case 1
  CHECK(hash::to_hex(hash::hmac_sha256(reinterpret_cast<const std::uint8_t*>(key.data()), key.size(), "Hi There")) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  const std::string long_key(131, '\xaa');   // RFC 4231, test case 6: a key longer than a block
  CHECK(hash::to_hex(hash::hmac_sha256(reinterpret_cast<const std::uint8_t*>(long_key.data()), long_key.size(), "Test Using Larger Than Block-Size Key - Hash Key First")) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
  std::string bytes;
  CHECK(hash::from_hex("00ff10", bytes) && bytes.size() == 3 && !hash::from_hex("0G", bytes) && !hash::from_hex("abc", bytes) && !hash::from_hex("AB", bytes));
  CHECK(hash::same_text("abc", "abc") && !hash::same_text("abc", "abd") && !hash::same_text("abc", "ab"));
}

static void test_the_signature_is_the_servers() {
  alarm::ZoneFeed feed;
  feed.zone_id = "z1"; feed.state = alarm::ZoneState::kTriggered; feed.battery_low = false; feed.seq = 1700000000000ULL;
  CHECK(alarm::zone_feed_signature(kKey, "alarm-1", feed) == kSignature);
  CHECK(alarm::zone_feed_signature(kKey, "alarm-2", feed) != kSignature);      // the node is part of what is signed
  feed.battery_low = true;
  CHECK(alarm::zone_feed_signature(kKey, "alarm-1", feed) != kSignature);
  CHECK(alarm::zone_feed_signature("not-a-key", "alarm-1", feed).empty());
}

static void test_the_zone_message() {
  alarm::ZoneFeed feed;
  const std::string good = std::string("{\"zone\":\"z1\",\"state\":\"triggered\",\"battery_low\":false,\"seq\":1700000000000,\"sig\":\"") + kSignature + "\"}";
  CHECK(alarm::parse_zone_feed(good, feed) && feed.signed_message && feed.seq == 1700000000000ULL && feed.sig == kSignature);
  CHECK(alarm::parse_zone_feed("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false}", feed) && !feed.signed_message);
  // seq and sig come together, and each must be what it says it is
  CHECK(!alarm::parse_zone_feed("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false,\"seq\":5}", feed));
  CHECK(!alarm::parse_zone_feed(std::string("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false,\"sig\":\"") + kSignature + "\"}", feed));
  CHECK(!alarm::parse_zone_feed(std::string("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false,\"seq\":-1,\"sig\":\"") + kSignature + "\"}", feed));
  CHECK(!alarm::parse_zone_feed(std::string("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false,\"seq\":1.5,\"sig\":\"") + kSignature + "\"}", feed));
  CHECK(!alarm::parse_zone_feed("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false,\"seq\":5,\"sig\":\"abc\"}", feed));
  CHECK(!alarm::parse_zone_feed(std::string("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false,\"seq\":5,\"sig\":\"") + std::string(64, 'A') + "\"}", feed));   // capitals are not the contract's
}

static void test_the_policy_message_is_strict() {
  alarm::Policy policy;
  CHECK(!policy.heard && !policy.config_hash && policy.zone_feed && !policy.zone_feed_signature);   // what the node does before the server has spoken
  CHECK(alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":false,\"zone_feed_signature\":true}", policy) && policy.heard && policy.config_hash && !policy.zone_feed && policy.zone_feed_signature);
  alarm::Policy untouched;
  CHECK(!alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true}", untouched) && !untouched.heard);                                       // an option missing
  CHECK(!alarm::parse_policy("{\"config_hash\":\"yes\",\"zone_feed\":true,\"zone_feed_signature\":false}", untouched));                           // not a flag
  CHECK(!alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"colour\":\"red\"}", untouched));   // an option that does not exist
  CHECK(!alarm::parse_policy("[]", untouched) && !alarm::parse_policy("nope", untouched) && !alarm::parse_policy("{\"config_hash\":true,\"config_hash\":false,\"zone_feed\":true}", untouched));
  CHECK(!untouched.heard);
  CHECK(!alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"sometimes\"}", untouched));   // not one of the three words
  CHECK(!alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":true}", untouched) && !untouched.heard);
  CHECK(alarm::policy_text(policy) == "{\"config_hash\":true,\"zone_feed\":false,\"zone_feed_signature\":true}");
  alarm::Policy back;
  CHECK(alarm::parse_policy(alarm::policy_text(policy), back) && back.config_hash == policy.config_hash && back.zone_feed == policy.zone_feed);
}

static void test_the_node_can_fix_each_option() {
  alarm::Policy server_says;
  server_says.heard = true; server_says.config_hash = true; server_says.zone_feed = false; server_says.zone_feed_signature = true;
  config::ServerLink link;   // the defaults: follow the server
  alarm::Effective now = alarm::effective(link, server_says);
  CHECK(now.config_hash && !now.zone_feed && !now.signature_required);   // no key on the node yet: it cannot enforce signatures, so the feed is not shut off by an update
  link.zone_feed_key = kKey;
  now = alarm::effective(link, server_says);
  CHECK(now.config_hash && !now.zone_feed && now.signature_required);    // with the key, the server's wish binds
  link.config_hash = config::Choice::kOff; link.zone_feed = config::Choice::kOn; link.zone_signature = config::Choice::kOff;
  now = alarm::effective(link, server_says);
  CHECK(!now.config_hash && now.zone_feed && !now.signature_required);   // the node's own word wins whatever the server says
  link = config::ServerLink{};
  now = alarm::effective(link, alarm::Policy{});
  CHECK(!now.config_hash && now.zone_feed && !now.signature_required);   // nothing heard, nothing fixed: what the node always did
}

static void test_the_settings_of_the_node() {
  config::Settings s = panel_settings();
  CHECK(s.server.config_hash == config::Choice::kPolicy && s.server.zone_feed == config::Choice::kPolicy && s.server.zone_signature == config::Choice::kPolicy && s.server.zone_feed_key.empty());
  CHECK(config::validate(s).empty());
  config::Problems problems;
  config::Settings edited;
  CHECK(config::load(std::string("{\"server\":{\"config_hash\":\"on\",\"zone_feed\":\"off\",\"zone_signature\":\"on\",\"zone_feed_key\":\"") + kKey + "\"}}", s, edited, problems));
  CHECK(edited.server.config_hash == config::Choice::kOn && edited.server.zone_feed == config::Choice::kOff && edited.server.zone_signature == config::Choice::kOn && edited.server.zone_feed_key == kKey);
  const std::string shown = config::to_json(edited, false);
  CHECK(shown.find(kKey) == std::string::npos && shown.find("\"zone_feed_key_set\":true") != std::string::npos && shown.find("\"config_hash\":\"on\"") != std::string::npos);
  config::Settings stored;
  problems.clear();
  CHECK(config::load(config::to_json(edited, true), s, stored, problems) && stored.server.zone_feed_key == kKey && stored.server.zone_signature == config::Choice::kOn);   // flash keeps the key
  // a document without the key keeps it; "_clear" erases it
  problems.clear();
  CHECK(config::load("{\"server\":{\"zone_feed\":\"on\"}}", edited, stored, problems) && stored.server.zone_feed_key == kKey);
  problems.clear();
  CHECK(!config::load("{\"server\":{\"zone_feed_key_clear\":true}}", edited, stored, problems));   // the signature is required and there would be no key to check it with
  problems.clear();
  CHECK(!config::load("{\"server\":{\"config_hash\":\"sometimes\"}}", s, stored, problems));
  problems.clear();
  CHECK(!config::load("{\"server\":{\"zone_feed_key\":\"abc\"}}", s, stored, problems));
  problems.clear();
  CHECK(!config::load(std::string("{\"server\":{\"zone_feed_key\":\"") + std::string(64, 'G') + "\"}}", s, stored, problems));
  problems.clear();
  CHECK(!config::load("{\"server\":{\"zone_signature\":\"on\"}}", s, stored, problems));         // on, with no key to check it with
}

static void test_the_hash_of_the_configuration() {
  const config::Settings s = panel_settings();
  const std::string first = alarm::config_fingerprint(s);
  CHECK(first.size() == 32 && first == alarm::config_fingerprint(panel_settings()));
  config::Settings other = s;
  other.panel.pin = "4321";
  CHECK(alarm::config_fingerprint(other) != first);                                       // another PIN
  other = s; other.zones[1].id = "garage";
  CHECK(alarm::config_fingerprint(other) != first);                                       // another zone name
  other = s; other.zones[2].enabled = true; other.zones[2].id = "hall";
  CHECK(alarm::config_fingerprint(other) != first);                                       // one more zone
  other = s; other.zones[0].kind = alarm::ZoneKind::kEntry;
  CHECK(alarm::config_fingerprint(other) != first);                                       // a zone that now behaves otherwise
  other = s; other.panel.timing.exit_delay_s = 30; other.sta.ssid = "otra";
  CHECK(alarm::config_fingerprint(other) == first);                                       // the delays and the Wi-Fi do not change what was armed
  // the decision
  CHECK(alarm::may_restore(false, "other", first));                                      // the option is off: as always
  CHECK(alarm::may_restore(true, "", first));                                            // saved before there was a hash: it comes back
  CHECK(alarm::may_restore(true, first, first));
  CHECK(!alarm::may_restore(true, "00000000000000000000000000000000", first));            // the zones or the PIN changed since: not restored
}

static void test_the_checks_of_the_feed() {
  config::ServerLink link;
  link.zone_feed_key = kKey;
  alarm::Effective now;
  now.zone_feed = true; now.signature_required = true;
  std::uint64_t last = 0;
  alarm::ZoneFeed feed;
  CHECK(alarm::parse_zone_feed(signed_feed("triggered", 1700000000000ULL), feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kAccepted && last == 1700000000000ULL);
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kReplay);                 // the same message again
  CHECK(alarm::parse_zone_feed(signed_feed("normal", 1699999999999ULL), feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kReplay);                 // an older one
  CHECK(alarm::parse_zone_feed(signed_feed("normal", 1700000000001ULL), feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kAccepted && last == 1700000000001ULL);
  // another key, another node, or a changed word: the signature does not hold, and the sequence number does not move
  CHECK(alarm::parse_zone_feed(signed_feed("tamper", 1700000000009ULL, std::string(64, '1')), feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kBadSignature && last == 1700000000001ULL);
  CHECK(alarm::parse_zone_feed(signed_feed("tamper", 1700000000009ULL, kKey, "alarm-2"), feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kBadSignature);
  CHECK(alarm::parse_zone_feed(signed_feed("normal", 1700000000009ULL), feed));
  feed.state = alarm::ZoneState::kTamper;                                                                           // the state changed on the way
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kBadSignature);
  // unsigned, with the signature required
  CHECK(alarm::parse_zone_feed("{\"zone\":\"z1\",\"state\":\"tamper\",\"battery_low\":false}", feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kUnsigned);
  // with the signature not required an unsigned message counts (as before), and a signed one is still checked when the node holds the key
  now.signature_required = false;
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kAccepted);
  CHECK(alarm::parse_zone_feed(signed_feed("tamper", 1700000000009ULL, std::string(64, '1')), feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kBadSignature);
  // no key on the node: a signed message cannot be checked; it counts only when the signature is not required
  config::ServerLink keyless;
  CHECK(alarm::check_zone_feed(now, keyless, "alarm-1", feed, last) == alarm::FeedVerdict::kAccepted);
  now.signature_required = true;
  CHECK(alarm::check_zone_feed(now, keyless, "alarm-1", feed, last) == alarm::FeedVerdict::kNoKey);
  // the feed switched off
  now.zone_feed = false;
  CHECK(alarm::parse_zone_feed("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false}", feed));
  CHECK(alarm::check_zone_feed(now, link, "alarm-1", feed, last) == alarm::FeedVerdict::kOff);
}

static void test_the_runner_obeys_the_options() {
  config::Settings s = panel_settings();
  auto armed_panel = [](const config::Settings& settings, std::uint64_t& now) {
    alarm::Runner node(settings);
    now = 0;
    node.tick(now);
    node.handle_zone_feed("{\"zone\":\"z1\",\"state\":\"normal\",\"battery_low\":false}", now);
    for (int i = 0; i < 20; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
    return node;
  };
  const std::string tamper = "{\"zone\":\"z1\",\"state\":\"tamper\",\"battery_low\":false}";
  std::uint64_t now = 0;
  // nothing heard and nothing fixed: the feed works as it always did
  {
    alarm::Runner node = armed_panel(s, now);
    CHECK(node.arm(alarm::Mode::kAway, false, now) == alarm::Refusal::kNone);
    for (int i = 0; i < 30; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
    CHECK(node.handle_zone_feed(tamper, now) && node.zones()[0].state == alarm::ZoneState::kTamper && node.feeds_refused() == 0);
  }
  // the server asks for signed messages (and the key is on the node): an unsigned tamper is refused, a signed one is taken, a replay is not
  s.server.zone_feed_key = kKey;
  {
    alarm::Runner node = armed_panel(s, now);
    CHECK(node.handle_policy("{\"config_hash\":false,\"zone_feed\":true,\"zone_feed_signature\":true}"));
    CHECK(!node.handle_zone_feed(tamper, now) && node.last_feed_verdict() == alarm::FeedVerdict::kUnsigned && node.zones()[0].state == alarm::ZoneState::kNormal);
    CHECK(node.handle_zone_feed(signed_feed("triggered", 1700000000000ULL), now) && node.zones()[0].state == alarm::ZoneState::kTriggered);
    CHECK(!node.handle_zone_feed(signed_feed("normal", 1700000000000ULL), now) && node.last_feed_verdict() == alarm::FeedVerdict::kReplay && node.zones()[0].state == alarm::ZoneState::kTriggered);   // a recorded "normal" does not mask it
    CHECK(node.feeds_refused() == 2);
    // the server turns the feed off
    CHECK(node.handle_policy("{\"config_hash\":false,\"zone_feed\":false,\"zone_feed_signature\":false}"));
    CHECK(!node.handle_zone_feed(signed_feed("normal", 1700000000005ULL), now) && node.last_feed_verdict() == alarm::FeedVerdict::kOff);
  }
  // the node's own choice beats the server's
  s.server.zone_feed = config::Choice::kOn;
  s.server.zone_signature = config::Choice::kOff;
  {
    alarm::Runner node = armed_panel(s, now);
    CHECK(node.handle_policy("{\"config_hash\":false,\"zone_feed\":false,\"zone_feed_signature\":true}"));
    CHECK(node.handle_zone_feed(tamper, now) && node.zones()[0].state == alarm::ZoneState::kTamper);
  }
  // a message that is not the contract's policy changes nothing, and the policy read back from flash is applied before the server speaks
  {
    alarm::Runner node(panel_settings());
    CHECK(!node.handle_policy("{\"config_hash\":true}") && !node.policy().heard);
    alarm::Policy stored;
    CHECK(alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false}", stored));
    node.set_policy(stored);
    CHECK(node.effective().config_hash);
  }
}

static void test_the_pin_to_disarm() {
  // the message of the older form says nothing about the PIN: "if_set", which is what the panel always did
  alarm::Policy policy;
  CHECK(policy.disarm_pin == alarm::DisarmPin::kIfSet);
  CHECK(alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"never\"}", policy) && policy.disarm_pin == alarm::DisarmPin::kNever && policy.heard);
  CHECK(alarm::policy_text(policy) == "{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"never\"}");
  alarm::Policy back;
  CHECK(alarm::parse_policy(alarm::policy_text(policy), back) && back.disarm_pin == alarm::DisarmPin::kNever);
  CHECK(alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"always\"}", back) && back.disarm_pin == alarm::DisarmPin::kAlways);
  CHECK(alarm::parse_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false}", back) && back.disarm_pin == alarm::DisarmPin::kIfSet);   // a message without it says "if_set" again
  CHECK(alarm::policy_text(back) == "{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false}");                                                   // and is written as the older form
  CHECK(alarm::parse_disarm_pin("if_set", back.disarm_pin) && !alarm::parse_disarm_pin("IF_SET", back.disarm_pin) && !alarm::parse_disarm_pin("", back.disarm_pin));
  // the node's own choice beats the server's
  config::ServerLink link;
  alarm::Policy never;
  never.heard = true; never.disarm_pin = alarm::DisarmPin::kNever;
  CHECK(alarm::effective(link, never).disarm_pin == alarm::DisarmPin::kNever && !alarm::effective(link, never).pin_asked());
  link.disarm_pin = config::PinChoice::kIfSet;
  CHECK(alarm::effective(link, never).disarm_pin == alarm::DisarmPin::kIfSet && alarm::effective(link, never).pin_asked());   // a message of the broker does not lower what is fixed here
  link.disarm_pin = config::PinChoice::kAlways;
  CHECK(alarm::effective(link, never).disarm_pin == alarm::DisarmPin::kAlways);
  link.disarm_pin = config::PinChoice::kNever;
  CHECK(!alarm::effective(link, alarm::Policy()).pin_asked());
  // the settings: the default follows the server, a bad word is refused, the choice is shown and stored
  config::Settings s = panel_settings();
  CHECK(s.server.disarm_pin == config::PinChoice::kPolicy);
  config::Problems problems;
  config::Settings edited;
  CHECK(config::load("{\"server\":{\"disarm_pin\":\"always\"}}", s, edited, problems) && edited.server.disarm_pin == config::PinChoice::kAlways);
  CHECK(config::to_json(edited, false).find("\"disarm_pin\":\"always\"") != std::string::npos);
  config::Settings stored;
  problems.clear();
  CHECK(config::load(config::to_json(edited, true), s, stored, problems) && stored.server.disarm_pin == config::PinChoice::kAlways);
  problems.clear();
  CHECK(!config::load("{\"server\":{\"disarm_pin\":\"sometimes\"}}", s, stored, problems));
  // what the panel does: with the PIN asked a wrong one is refused and counted, with "never" the disarm and the silence go through without one and nobody is locked out
  const std::string never_message = "{\"config_hash\":false,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"never\"}";
  const std::string always_message = "{\"config_hash\":false,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"always\"}";
  const std::string old_message = "{\"config_hash\":false,\"zone_feed\":true,\"zone_feed_signature\":false}";
  auto armed = [](const config::Settings& settings, std::uint64_t& now) {
    alarm::Runner node(settings);
    now = 0;
    node.tick(now);
    for (int i = 0; i < 20; ++i) { node.feed(1, 800, now); now += 100; node.tick(now); }
    CHECK(node.arm(alarm::Mode::kAway, true, now) == alarm::Refusal::kNone);
    now += 5000; node.tick(now);
    return node;
  };
  std::uint64_t now = 0;
  {
    alarm::Runner node = armed(panel_settings(), now);
    CHECK(node.pin_asked());
    CHECK(node.disarm("", now) == alarm::Refusal::kBadPin && node.panel().phase() != alarm::Phase::kDisarmed);
    CHECK(node.disarm("0000", now) == alarm::Refusal::kBadPin);
    CHECK(node.disarm("1234", now) == alarm::Refusal::kNone && node.panel().phase() == alarm::Phase::kDisarmed);
  }
  {
    alarm::Runner node = armed(panel_settings(), now);
    CHECK(node.handle_policy(always_message));
    CHECK(node.disarm("0000", now) == alarm::Refusal::kBadPin && node.disarm("1234", now) == alarm::Refusal::kNone);
  }
  {
    alarm::Runner node = armed(panel_settings(), now);
    CHECK(node.handle_policy(never_message) && !node.pin_asked());
    CHECK(node.disarm("", now) == alarm::Refusal::kNone && node.panel().phase() == alarm::Phase::kDisarmed);   // no PIN, no refusal
    CHECK(!node.locked_out(now));
  }
  {
    // three wrong PINs lock the panel while the PIN is asked; once the server says "never" the panel opens again, and when it says "if_set" the PIN is asked as before
    alarm::Runner node = armed(panel_settings(), now);
    for (int i = 0; i < 3; ++i) node.disarm("0000", now);
    CHECK(node.locked_out(now) && node.disarm("1234", now) == alarm::Refusal::kLockedOut);
    CHECK(node.handle_policy(never_message));
    CHECK(node.disarm("", now) == alarm::Refusal::kNone && node.panel().phase() == alarm::Phase::kDisarmed);
    CHECK(node.handle_policy(old_message) && node.pin_asked());
  }
  {
    // fixed on the node: a message of the broker that says "never" changes nothing
    config::Settings fixed = panel_settings();
    fixed.server.disarm_pin = config::PinChoice::kAlways;
    alarm::Runner node = armed(fixed, now);
    CHECK(node.handle_policy(never_message) && node.pin_asked());
    CHECK(node.disarm("", now) == alarm::Refusal::kBadPin && node.disarm("1234", now) == alarm::Refusal::kNone);
  }
}

int main() {
  test_hashes_against_known_values();
  test_the_signature_is_the_servers();
  test_the_zone_message();
  test_the_policy_message_is_strict();
  test_the_node_can_fix_each_option();
  test_the_settings_of_the_node();
  test_the_hash_of_the_configuration();
  test_the_checks_of_the_feed();
  test_the_runner_obeys_the_options();
  test_the_pin_to_disarm();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
