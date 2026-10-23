// ARMOR-ALARM - host tests of what the alarm task decides around the panel (core/alarm_supervisor.hpp): what comes back from flash at start, what is written and when, when the state is said again,
// and the rules of the panel seen through it: arming and disarming, the PIN and its lockout, the delays and the siren's timer. No hardware was used.
#include <cstdio>
#include <string>

#include "../core/alarm_config.hpp"
#include "../core/alarm_supervisor.hpp"

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
using alarm::BootResult;
using alarm::Mode;
using alarm::Phase;
using alarm::Refusal;

static const char* kHashOn = "{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false}";
static const char* kHashOff = "{\"config_hash\":false,\"zone_feed\":true,\"zone_feed_signature\":false}";

// Two digital zones (normally closed contacts: a high pin is a triggered zone), no hold time, short delays.
static config::Settings house(const std::string& pin = "1234") {
  config::Settings s = config::default_settings("a1b2c3");
  s.node_id = "alarm-1";
  for (std::size_t i = 0; i < 2; ++i) {
    config::ZoneSetting& z = s.zones[i];
    z.enabled = true; z.id = i == 0 ? "door" : "hall"; z.name = z.id; z.input = "digital"; z.pin = 4 + static_cast<int>(i); z.hold_ms = 0;
    z.kind = i == 0 ? alarm::ZoneKind::kEntry : alarm::ZoneKind::kInstant;
  }
  s.panel.pin = pin;
  s.panel.timing.exit_delay_s = 10; s.panel.timing.entry_delay_s = 10; s.panel.timing.siren_max_s = 60; s.panel.timing.lockout_after = 3; s.panel.timing.lockout_s = 30;
  s.server.commands = true;
  return s;
}

static alarm::Stored with_state(const char* text) { alarm::Stored s; s.state = text; return s; }

// Both contacts closed (all is well); the node has real time, so the events are kept for flash.
static void closed(alarm::Supervisor& sup, std::uint64_t now) { sup.runner().set_epoch(1700000000); sup.runner().feed_digital(0, false, now); sup.runner().feed_digital(1, false, now); }

// What a turn of the task does with what the supervisor asked for: everything is written.
static void apply(alarm::Supervisor& sup, const alarm::Writes& w, std::uint64_t now) {
  if (!w.events.empty()) sup.events_written(w.events, now);
  if (w.profile) sup.profile_written(w.profile_text);
  if (!w.state.empty()) { sup.state_written(w.state); if (w.bind_hash) sup.hash_written(); }
  else if (w.hash_only) sup.hash_written();
}

static void test_nothing_saved() {
  alarm::Supervisor sup(house());
  CHECK(sup.boot(alarm::Stored{}, 0) == BootResult::kNothingSaved);
  CHECK(sup.runner().panel().phase() == Phase::kDisarmed);
  CHECK(!sup.plan_writes(1000).any());   // a fresh disarmed panel has nothing to write
  CHECK(sup.config_hash().size() == 32);
}

static void test_restore_armed_state() {
  alarm::Supervisor sup(house());
  alarm::Stored stored;
  stored.state = "armed|away|";
  CHECK(sup.boot(stored, 5000) == BootResult::kRestored);
  CHECK(sup.runner().panel().phase() == Phase::kArmed && sup.runner().panel().mode() == Mode::kAway);
  CHECK(!sup.plan_writes(5100).any());   // it is what flash holds already

  alarm::Supervisor stay(house());
  stored.state = "armed|stay|hall";
  CHECK(stay.boot(stored, 0) == BootResult::kRestored && stay.runner().panel().mode() == Mode::kStay && stay.runner().panel().bypassed("hall"));

  alarm::Supervisor alarmed(house());
  stored.state = "alarm|away|";
  CHECK(alarmed.boot(stored, 0) == BootResult::kRestored && alarmed.runner().panel().phase() == Phase::kAlarm && alarmed.runner().siren());

  alarm::Supervisor disarmed(house());
  stored.state = "disarmed|disarmed|";
  CHECK(disarmed.boot(stored, 0) == BootResult::kStateNotArmed && disarmed.runner().panel().phase() == Phase::kDisarmed);

  for (const char* bad : {"armed|away|x|nonsense", "garbage", "armed|sideways|", "disarmed|away|", "armed"}) {
    alarm::Supervisor sup2(house());
    stored.state = bad;
    CHECK(sup2.boot(stored, 0) == BootResult::kStateUnreadable && sup2.runner().panel().phase() == Phase::kDisarmed);
  }
}

static void test_restore_with_profile() {
  alarm::ArmProfile profile;
  profile.id = "night"; profile.name = "Night"; profile.stay = false; profile.exit_delay_s = 3; profile.entry_delay_s = 4; profile.siren = false;
  alarm::Supervisor sup(house());
  alarm::Stored stored;
  stored.state = "armed|away|";
  stored.profile = alarm::profile_json(profile);
  CHECK(sup.boot(stored, 0) == BootResult::kRestored);
  CHECK(sup.runner().profile() != nullptr && sup.runner().profile()->id == "night");
  CHECK(!sup.plan_writes(100).profile);   // the profile in flash is the one armed with

  alarm::Supervisor stay(house());   // a profile of the other mode is not taken
  stored.state = "armed|stay|";
  CHECK(stay.boot(stored, 0) == BootResult::kRestored && stay.runner().profile() == nullptr);
  alarm::Writes w = stay.plan_writes(100);
  CHECK(w.profile && w.profile_text.empty());   // flash still holds a profile nobody uses: it is erased, so a later arming without one cannot bring it back

  alarm::Supervisor broken(house());
  stored.state = "armed|away|";
  stored.profile = "{not json";
  CHECK(broken.boot(stored, 0) == BootResult::kRestored && broken.runner().profile() == nullptr);
}

static void test_config_hash_binding() {
  const config::Settings settings = house();
  alarm::Supervisor first(settings);
  const std::string hash = first.config_hash();

  // the option is on and the state was saved under another configuration: not restored, disarmed, and the disarmed state is written over it
  alarm::Supervisor changed(settings);
  alarm::Stored stored;
  stored.state = "armed|away|";
  stored.policy = kHashOn;
  stored.hash = "00000000000000000000000000000000";
  CHECK(changed.boot(stored, 0) == BootResult::kNotRestoredConfigChanged && changed.runner().panel().phase() == Phase::kDisarmed);
  alarm::Writes w = changed.plan_writes(100);
  CHECK(w.state == "disarmed|disarmed|" && w.bind_hash);   // flash still held the old state: the disarmed one goes over it, with the hash of this configuration
  // the same configuration comes back
  alarm::Supervisor same(settings);
  stored.hash = hash;
  CHECK(same.boot(stored, 0) == BootResult::kRestored);
  CHECK(!same.plan_writes(100).any());

  // the option is off: the saved state comes back whatever the hash says
  alarm::Supervisor off(settings);
  stored.policy = kHashOff;
  stored.hash = "00000000000000000000000000000000";
  CHECK(off.boot(stored, 0) == BootResult::kRestored);
  // a state saved before the option existed has no hash: it comes back even with the option on
  alarm::Supervisor old(settings);
  stored.policy = kHashOn;
  stored.hash.clear();
  CHECK(old.boot(stored, 0) == BootResult::kRestored);

  // a different PIN or a different zone gives a different hash
  config::Settings other_pin = settings;
  other_pin.panel.pin = "4321";
  config::Settings other_zone = settings;
  other_zone.zones[1].kind = alarm::ZoneKind::kAlways;
  CHECK(alarm::Supervisor(other_pin).config_hash() != hash && alarm::Supervisor(other_zone).config_hash() != hash);
  CHECK(alarm::Supervisor(settings).config_hash() == hash);

  // with the option on, a state written is written with its hash; the option turned on later writes the hash alone
  alarm::Supervisor bound(settings);
  std::string to_write;
  CHECK(bound.on_policy(kHashOn, to_write) && !to_write.empty());
  closed(bound, 0);
  CHECK(bound.runner().arm(Mode::kAway, false, 1000) == Refusal::kNone);
  w = bound.plan_writes(1000);
  CHECK(w.state == "armed|away|" && w.bind_hash && !w.hash_only);
  apply(bound, w, 1000);
  CHECK(!bound.plan_writes(1100).any());

  alarm::Supervisor late(settings);
  late.boot(with_state("armed|away|"), 0);   // restored with no hash and the option off
  CHECK(!late.plan_writes(100).any());
  CHECK(late.on_policy(kHashOn, to_write));
  w = late.plan_writes(200);
  CHECK(w.state.empty() && w.hash_only);
  apply(late, w, 200);
  CHECK(!late.plan_writes(300).any());
  // a write that failed is asked for again
  alarm::Supervisor retry(settings);
  retry.boot(with_state("armed|away|"), 0);
  retry.on_policy(kHashOn, to_write);
  CHECK(retry.plan_writes(1).hash_only && retry.plan_writes(2).hash_only);
}

static void test_policy_message() {
  alarm::Supervisor sup(house());
  std::string to_write;
  CHECK(!sup.on_policy("{}", to_write) && to_write.empty());
  CHECK(!sup.on_policy("{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false,\"disarm_pin\":\"never\"}", to_write));   // the node has no option for the PIN of the server's users
  CHECK(!sup.runner().effective().config_hash);
  CHECK(sup.on_policy(kHashOn, to_write) && to_write == "{\"config_hash\":true,\"zone_feed\":true,\"zone_feed_signature\":false}");
  CHECK(sup.runner().effective().config_hash);
  sup.policy_written(to_write);
  CHECK(sup.on_policy(kHashOn, to_write) && to_write.empty());   // the same again: nothing to write
  CHECK(sup.on_policy(kHashOff, to_write) && !to_write.empty());
  // the policy in flash is applied at start
  alarm::Supervisor boot(house());
  alarm::Stored stored;
  stored.policy = kHashOn;
  boot.boot(stored, 0);
  CHECK(boot.runner().effective().config_hash && boot.runner().policy().heard);
  CHECK(boot.on_policy(kHashOn, to_write) && to_write.empty());   // already what flash holds
  stored.policy = "garbage";
  alarm::Supervisor bad(house());
  bad.boot(stored, 0);
  CHECK(!bad.runner().policy().heard);
}

static void test_arm_disarm_and_writes() {
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  CHECK(sup.runner().arm(Mode::kAway, false, 1000) == Refusal::kNone && sup.runner().panel().phase() == Phase::kExitDelay);
  alarm::Writes w = sup.plan_writes(1000);
  CHECK(w.state == "armed|away|" && !w.bind_hash && !w.profile && w.events.empty());   // an exit delay is saved as armed at once; its event is not one that matters, so it waits for the half minute
  apply(sup, w, 1000);
  sup.runner().tick(5000);
  CHECK(sup.runner().panel().phase() == Phase::kExitDelay);
  sup.runner().tick(11000);
  CHECK(sup.runner().panel().phase() == Phase::kArmed);
  w = sup.plan_writes(11000);
  CHECK(w.state.empty() && !w.events.empty());   // armed|away| is already in flash; the armed event matters and goes at once
  apply(sup, w, 11000);
  CHECK(sup.runner().disarm("1234", 12000) == Refusal::kNone);
  w = sup.plan_writes(12000);
  CHECK(w.state == "disarmed|disarmed|" && !w.events.empty());
  apply(sup, w, 12000);
  CHECK(!sup.plan_writes(12100).any());
}

static void test_events_are_written_sparingly() {
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  // wrong PINs are not "important": the first one is written once the half minute since the start has passed, then at most every half minute
  const std::uint64_t t0 = 100000;
  CHECK(sup.runner().disarm("0000", t0) == Refusal::kBadPin);
  alarm::Writes w = sup.plan_writes(t0);
  CHECK(!w.events.empty());   // 100 s since the start: due
  apply(sup, w, t0);
  CHECK(sup.runner().disarm("1111", t0 + 1000) == Refusal::kBadPin);
  CHECK(sup.plan_writes(t0 + 1000).events.empty());
  CHECK(sup.plan_writes(t0 + alarm::kEventsWriteEveryMs - 1).events.empty());
  CHECK(!sup.plan_writes(t0 + alarm::kEventsWriteEveryMs).events.empty());
  // but something that changes the state of the house is written at once: the lockout (the third wrong PIN)
  CHECK(sup.runner().disarm("2222", t0 + 2000) == Refusal::kBadPin);   // 3rd wrong PIN: locked out
  CHECK(sup.runner().locked_out(t0 + 2000));
  w = sup.plan_writes(t0 + 2000);
  CHECK(!w.events.empty() && w.state == "disarmed|disarmed||lock1");
  apply(sup, w, t0 + 2000);
  // a write that failed is asked for again on the next turn
  CHECK(sup.runner().arm(Mode::kAway, false, t0 + 3000) == Refusal::kNone);
  CHECK(sup.plan_writes(t0 + 3000).events.empty());   // the exit delay is not an event that matters: it waits for the half minute
  sup.runner().tick(t0 + 14000);
  CHECK(!sup.plan_writes(t0 + 14000).events.empty() && !sup.plan_writes(t0 + 14100).events.empty());   // armed: at once, and again while the write has not been told done
  // events from before the restart come back
  alarm::Supervisor back(house());
  alarm::Stored stored;
  stored.events = alarm::events_text(sup.runner().panel().events());
  back.boot(stored, 0);
  CHECK(!back.runner().panel().events().empty() && back.runner().panel().events().front().restored);
}

static void test_forget_stops_every_write() {
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  sup.runner().arm(Mode::kAway, false, 1000);
  CHECK(sup.plan_writes(1000).any());
  sup.forget();
  CHECK(sup.forgotten() && !sup.plan_writes(2000).any() && !sup.plan_writes(999999).any());
}

static void test_profile_follows_the_arming() {
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  alarm::ArmProfile profile;
  profile.id = "short"; profile.name = "Short"; profile.stay = false; profile.exit_delay_s = 2; profile.entry_delay_s = 2;
  const std::string command = "{\"kind\":\"alarm_command\",\"node_id\":\"alarm-1\",\"timestamp_ms\":7000,\"command_id\":\"c0ffee0123456789\",\"action\":\"arm\",\"mode\":\"away\",\"profile\":" + alarm::profile_json(profile) + "}";
  const alarm::Reply reply = sup.runner().handle(command, 1000);
  CHECK(reply.understood && reply.refusal == Refusal::kNone && sup.runner().profile() != nullptr);
  alarm::Writes w = sup.plan_writes(1000);
  CHECK(w.profile && w.profile_text == alarm::profile_json(profile) && w.state == "armed|away|");
  apply(sup, w, 1000);
  CHECK(!sup.plan_writes(1100).profile);
  sup.runner().tick(3500);   // the profile's exit delay (2 s), not the node's (10 s)
  CHECK(sup.runner().panel().phase() == Phase::kArmed);
  // disarming erases the profile from flash
  const std::string disarm = "{\"kind\":\"alarm_command\",\"node_id\":\"alarm-1\",\"timestamp_ms\":8000,\"command_id\":\"c0ffee0123456780\",\"action\":\"disarm\"}";
  CHECK(sup.runner().handle(disarm, 5000).refusal == Refusal::kNone);
  w = sup.plan_writes(5000);
  CHECK(w.profile && w.profile_text.empty());
}

static void test_state_message_timer() {
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  const std::string first = sup.state_message(false, 1, 1000);
  CHECK(!first.empty() && first.find("\"phase\":\"disarmed\"") != std::string::npos);
  CHECK(sup.state_message(false, 2, 1000 + alarm::kPublishEveryMs - 1).empty());   // nothing changed and the time is not up
  CHECK(!sup.state_message(false, 3, 1000 + alarm::kPublishEveryMs).empty());      // said again every few seconds
  CHECK(sup.state_message(false, 4, 1000 + alarm::kPublishEveryMs + 10).empty());
  CHECK(!sup.state_message(true, 5, 1000 + alarm::kPublishEveryMs + 20).empty());  // forced
  // a change is said at once: a zone opens
  sup.runner().feed_digital(1, true, 7000);
  CHECK(!sup.state_message(false, 6, 7000).empty());
  CHECK(sup.state_message(false, 7, 7100).empty());
  // arming, the lockout and the siren change the signature too
  const std::string before = sup.signature(8000);
  sup.runner().feed_digital(1, false, 8000);
  CHECK(sup.runner().arm(Mode::kAway, false, 8000) == Refusal::kNone);
  CHECK(sup.signature(8000) != before && !sup.state_message(false, 8, 8000).empty());
  for (int i = 0; i < 3; ++i) sup.runner().disarm("0000", 9000 + static_cast<std::uint64_t>(i));
  CHECK(!sup.state_message(false, 9, 9100).empty());   // the lockout
  CHECK(sup.signature(9100).find('L') != std::string::npos);
  CHECK(sup.signature(9100 + 31000).find('L') == std::string::npos);   // the lockout ends by itself with the time
}

static void test_pin_rules() {
  // the panel's PIN: wrong ones are counted, the lockout refuses even the right one and grows, a right one clears the count
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  sup.runner().arm(Mode::kAway, false, 0);
  sup.runner().tick(11000);
  CHECK(sup.runner().disarm("9999", 20000) == Refusal::kBadPin);
  CHECK(sup.runner().disarm("12345", 20001) == Refusal::kBadPin);   // another length
  CHECK(sup.runner().disarm("", 20002) == Refusal::kBadPin && sup.runner().locked_out(20003));     // the third wrong one locks out
  CHECK(sup.runner().disarm("1234", 20003) == Refusal::kLockedOut && sup.runner().panel().phase() == Phase::kArmed);
  CHECK(sup.runner().silence("1234", 20004) == Refusal::kLockedOut);
  CHECK(sup.runner().disarm("1234", 20003 + 30000) == Refusal::kNone && sup.runner().panel().phase() == Phase::kDisarmed);   // the first lockout lasts lockout_s

  // the server's command needs no PIN and is not held back by the lockout (the server decided who may disarm: its own alarm.disarm_pin setting)
  alarm::Supervisor server(house());
  server.boot(alarm::Stored{}, 0);
  closed(server, 0);
  server.runner().arm(Mode::kAway, false, 0);
  for (int i = 0; i < 3; ++i) server.runner().disarm("0000", 1000 + static_cast<std::uint64_t>(i));
  CHECK(server.runner().locked_out(1500));
  const std::string disarm = "{\"kind\":\"alarm_command\",\"node_id\":\"alarm-1\",\"timestamp_ms\":8000,\"command_id\":\"c0ffee0123456780\",\"action\":\"disarm\"}";
  CHECK(server.runner().handle(disarm, 1600).refusal == Refusal::kNone && server.runner().panel().phase() == Phase::kDisarmed);
  // ...and is ignored when the node's setting does not let the server command it
  config::Settings closed_to_server = house();
  closed_to_server.server.commands = false;
  alarm::Supervisor deaf(closed_to_server);
  CHECK(!deaf.runner().handle(disarm, 0).understood);

  // a panel with no PIN set can never be disarmed from its keypad, whatever is typed
  alarm::Supervisor nopin(house(""));
  nopin.boot(alarm::Stored{}, 0);
  closed(nopin, 0);
  nopin.runner().arm(Mode::kAway, false, 0);
  nopin.runner().tick(11000);
  CHECK(nopin.runner().disarm("", 12000) == Refusal::kBadPin && nopin.runner().disarm("0000", 12001) == Refusal::kBadPin);
  CHECK(nopin.runner().panel().phase() == Phase::kArmed);
}

static void test_timers() {
  alarm::Supervisor sup(house());
  sup.boot(alarm::Stored{}, 0);
  closed(sup, 0);
  // the exit delay: a zone that opens while leaving raises nothing
  CHECK(sup.runner().arm(Mode::kAway, false, 0) == Refusal::kNone);
  sup.runner().feed_digital(1, true, 5000);
  sup.runner().tick(5000);
  CHECK(sup.runner().panel().phase() == Phase::kExitDelay);
  sup.runner().feed_digital(1, false, 6000);
  sup.runner().tick(9999);
  CHECK(sup.runner().panel().phase() == Phase::kExitDelay);
  sup.runner().tick(10000);
  CHECK(sup.runner().panel().phase() == Phase::kArmed);
  // the entry delay: the entry door starts it, nobody disarms, the alarm rings when it runs out
  sup.runner().feed_digital(0, true, 20000);
  CHECK(sup.runner().panel().phase() == Phase::kEntryDelay);
  sup.runner().tick(29999);
  CHECK(sup.runner().panel().phase() == Phase::kEntryDelay && !sup.runner().siren());
  sup.runner().tick(30000);
  CHECK(sup.runner().panel().phase() == Phase::kAlarm && sup.runner().siren());
  // the alarm is saved as an alarm; the siren stops by itself after siren_max_s and the alarm stays latched
  CHECK(sup.plan_writes(30000).state == "alarm|away|");
  sup.runner().tick(30000 + 59999);
  CHECK(sup.runner().siren());
  sup.runner().tick(30000 + 60000);
  CHECK(!sup.runner().siren() && sup.runner().panel().phase() == Phase::kAlarm);
  CHECK(sup.plan_writes(90000).state == "alarm|away||silenced");
  // a restart then brings it back without the siren
  alarm::Supervisor back(house());
  alarm::Stored stored;
  stored.state = "alarm|away||silenced";
  CHECK(back.boot(stored, 0) == BootResult::kRestored && back.runner().panel().phase() == Phase::kAlarm && !back.runner().siren());
  // only a right PIN ends it
  CHECK(sup.runner().disarm("0000", 100000) == Refusal::kBadPin && sup.runner().panel().phase() == Phase::kAlarm);
  CHECK(sup.runner().disarm("1234", 100001) == Refusal::kNone && sup.runner().panel().phase() == Phase::kDisarmed);

  // arming is refused with a zone open, and with force the open zone is left out until disarmed
  alarm::Supervisor open(house());
  open.boot(alarm::Stored{}, 0);
  open.runner().feed_digital(0, false, 0);
  open.runner().feed_digital(1, true, 0);
  open.runner().feed_digital(1, true, 50);
  CHECK(open.runner().arm(Mode::kAway, false, 100) == Refusal::kZonesOpen);
  CHECK(open.runner().arm(Mode::kAway, true, 100) == Refusal::kNone && open.runner().panel().bypassed("hall"));
  CHECK(open.plan_writes(100).state == "armed|away|hall");
  CHECK(open.runner().arm(Mode::kAway, false, 200) == Refusal::kNotDisarmed);
}

int main() {
  test_nothing_saved();
  test_restore_armed_state();
  test_restore_with_profile();
  test_config_hash_binding();
  test_policy_message();
  test_arm_disarm_and_writes();
  test_events_are_written_sparingly();
  test_forget_stops_every_write();
  test_profile_follows_the_arming();
  test_state_message_timer();
  test_pin_rules();
  test_timers();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
