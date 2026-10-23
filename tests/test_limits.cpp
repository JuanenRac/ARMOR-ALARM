// ARMOR-NODE-ALARM - host tests of the corners of the panel and of what it reads: the PIN lockout and how it grows, the record a restart brings back, an arming profile and a command that are nearly
// right, and the readings of a wired loop at the edge of each limit.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// No loop, siren or broker is used: the zones are numbers the test sets, and the clock is a number the test moves.
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "../core/alarm_message.hpp"
#include "../core/alarm_panel.hpp"
#include "../core/alarm_zone.hpp"
#include "../core/arm_profile.hpp"
#include "../core/json.hpp"

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
using namespace armor::alarm;

static std::vector<Zone> house() {
  return {{"front-door", "Front door", ZoneKind::kEntry}, {"window", "Window", ZoneKind::kInstant}, {"hall", "Hall", ZoneKind::kInterior}, {"smoke", "Smoke", ZoneKind::kAlways}};
}
static Timing timing() {
  Timing t;
  t.exit_delay_s = 10; t.entry_delay_s = 10; t.siren_max_s = 60; t.lockout_after = 3; t.lockout_s = 60;
  return t;
}
static json::Value parsed(const std::string& text) {
  json::Value document;
  const bool ok = json::parse(text, document);
  CHECK(ok);
  return document;
}

// ---- the PIN and its lockout -----------------------------------------------------------------------------------------------------------------

static void test_lockout_grows_and_forgives() {
  Panel panel(timing(), house(), "1234");
  std::uint64_t now = 1000;
  panel.arm(Mode::kAway, false, now);
  panel.tick(now + 10'000);
  now += 10'000;
  CHECK(panel.phase() == Phase::kArmed);
  // two wrong PINs, then the right one: the count starts over
  CHECK(panel.disarm("0000", now) == Refusal::kBadPin && panel.disarm("1111", now) == Refusal::kBadPin && !panel.locked_out(now));
  CHECK(panel.disarm("1234", now) == Refusal::kNone && panel.phase() == Phase::kDisarmed);
  panel.arm(Mode::kAway, false, now);
  panel.tick(now + 10'000);
  now += 10'000;
  CHECK(panel.disarm("0000", now) == Refusal::kBadPin && panel.disarm("1111", now) == Refusal::kBadPin && panel.disarm("2222", now) == Refusal::kBadPin);   // the third in a row
  CHECK(panel.locked_out(now) && panel.disarm("1234", now) == Refusal::kLockedOut && panel.phase() == Phase::kArmed);   // even the right one is refused
  CHECK(panel.silence("1234", now) == Refusal::kLockedOut);
  CHECK(panel.locked_out(now + 59'999) && !panel.locked_out(now + 60'000));
  // the second lockout without a right PIN in between lasts five times as long, the third fifteen times, and so does every one after
  const std::uint64_t lengths_s[] = {300, 900, 900};
  for (const std::uint64_t seconds : lengths_s) {
    now += 60'000 + (seconds == 300 ? 0 : 0);
    panel.tick(now);
    CHECK(!panel.locked_out(now));
    for (int i = 0; i < 3; ++i) CHECK(panel.disarm("9999", now) == Refusal::kBadPin);
    CHECK(panel.locked_out(now + seconds * 1000 - 1) && !panel.locked_out(now + seconds * 1000));
    now += seconds * 1000;
    panel.tick(now);
  }
  // the right PIN forgives the growth: the next lockout is the short one again
  CHECK(panel.disarm("1234", now) == Refusal::kNone);
  panel.arm(Mode::kAway, false, now);
  panel.tick(now + 10'000);
  now += 10'000;
  for (int i = 0; i < 3; ++i) panel.disarm("0000", now);
  CHECK(panel.locked_out(now + 59'999) && !panel.locked_out(now + 60'000));
  // a wrong PIN to silence counts exactly like a wrong PIN to disarm
  Panel other(timing(), house(), "1234");
  other.arm(Mode::kAway, false, 0);
  other.tick(10'000);
  other.set_zone("window", ZoneState::kTriggered, 11'000);
  CHECK(other.phase() == Phase::kAlarm && other.siren());
  CHECK(other.silence("0000", 12'000) == Refusal::kBadPin && other.silence("0000", 12'000) == Refusal::kBadPin && other.disarm("0000", 12'000) == Refusal::kBadPin && other.locked_out(12'000));
  CHECK(other.siren());
  // the server's say-so is not held back by a lockout, and it clears the count
  CHECK(other.silence_authorized(13'000) == Refusal::kNone && !other.siren());
  CHECK(other.silence_authorized(13'001) == Refusal::kNone);                    // a siren already quiet stays so
  CHECK(other.disarm_authorized(14'000) == Refusal::kNone && other.phase() == Phase::kDisarmed && other.mode() == Mode::kDisarmed && !other.siren());
  CHECK(other.disarm_authorized(15'000) == Refusal::kNone);                     // already disarmed
  CHECK(other.silence_authorized(16'000) == Refusal::kNotArmed);                // nothing rings
  // a panel with no PIN set can be disarmed only by the server
  Panel nobody(timing(), house(), "");
  nobody.arm(Mode::kAway, false, 0);
  nobody.tick(10'000);
  CHECK(nobody.disarm("", 11'000) == Refusal::kBadPin && nobody.disarm("anything", 11'000) == Refusal::kBadPin);
  CHECK(nobody.disarm_authorized(12'000) == Refusal::kNone);
  // a PIN is compared whole: a longer or a shorter one is wrong
  for (const char* wrong : {"12345", "123", "", "1234 ", "4321"}) {
    Panel exact(timing(), house(), "1234");
    CHECK(exact.disarm(wrong, 0) == Refusal::kBadPin);
  }
  Panel exact(timing(), house(), "1234");
  CHECK(exact.disarm("1234", 0) == Refusal::kNone);
}

static void test_arming_rules() {
  Panel panel(timing(), house(), "1234");
  CHECK(panel.arm(Mode::kDisarmed, false, 0) == Refusal::kNotArmed);
  CHECK(panel.arm(Mode::kAway, false, 0) == Refusal::kNone && panel.phase() == Phase::kExitDelay);
  CHECK(panel.arm(Mode::kStay, false, 1) == Refusal::kNotDisarmed && panel.arm(Mode::kAway, false, 1) == Refusal::kNotDisarmed);
  // a smoke detector that is sounding stops an arming, forced or not
  Panel smoky(timing(), house(), "1234");
  smoky.set_zone("smoke", ZoneState::kTriggered, 0);
  CHECK(smoky.phase() == Phase::kAlarm);   // the always zone sounds with the system disarmed
  CHECK(smoky.arm(Mode::kAway, false, 1) == Refusal::kNotDisarmed);
  CHECK(smoky.disarm("1234", 2) == Refusal::kNone && smoky.phase() == Phase::kDisarmed);
  smoky.set_zone("smoke", ZoneState::kNormal, 3);
  smoky.set_zone("smoke", ZoneState::kTriggered, 4);
  smoky.disarm("1234", 5);
  CHECK(smoky.arm(Mode::kAway, false, 6) == Refusal::kZonesOpen && smoky.arm(Mode::kAway, true, 6) == Refusal::kZonesOpen && smoky.phase() == Phase::kDisarmed);
  // a smoke detector that cannot be read stops a forced arming too (it is the one zone that is never left out)
  Panel unreadable(timing(), house(), "1234");
  unreadable.set_zone("smoke", ZoneState::kTrouble, 0);
  CHECK(unreadable.phase() == Phase::kDisarmed);                                // a zone that cannot be read tells nothing: it never sounds
  CHECK(unreadable.arm(Mode::kAway, false, 1) == Refusal::kZonesOpen && unreadable.arm(Mode::kAway, true, 1) == Refusal::kZonesOpen);
  // another zone in trouble is left out by a forced arming and then ignored
  Panel trouble(timing(), house(), "1234");
  trouble.set_zone("window", ZoneState::kTrouble, 0);
  CHECK(trouble.arm(Mode::kAway, false, 1) == Refusal::kZonesOpen);
  CHECK(trouble.arm(Mode::kAway, true, 1) == Refusal::kNone && trouble.bypassed("window") && !trouble.bypassed("hall"));
  trouble.tick(11'000);
  trouble.set_zone("window", ZoneState::kTriggered, 12'000);
  CHECK(trouble.phase() == Phase::kArmed);                                       // a zone left out ignores its contact
  trouble.set_zone("window", ZoneState::kTamper, 13'000);
  CHECK(trouble.phase() == Phase::kAlarm);                                       // but never its sabotage
  // a tamper stops an arming however it is forced
  Panel sabotaged(timing(), house(), "1234");
  sabotaged.set_zone("window", ZoneState::kTamper, 0);
  CHECK(sabotaged.phase() == Phase::kAlarm);
  sabotaged.disarm("1234", 1);
  CHECK(sabotaged.arm(Mode::kAway, true, 2) == Refusal::kZonesOpen);
  // the interior zone matters in away, not in stay
  Panel stay(timing(), house(), "1234");
  stay.set_zone("hall", ZoneState::kTriggered, 0);
  CHECK(stay.arm(Mode::kAway, false, 1) == Refusal::kZonesOpen && stay.arm(Mode::kStay, false, 1) == Refusal::kNone);
  stay.tick(11'000);
  CHECK(stay.phase() == Phase::kArmed && stay.mode() == Mode::kStay);
  stay.set_zone("hall", ZoneState::kNormal, 12'000);
  stay.set_zone("hall", ZoneState::kTriggered, 13'000);
  CHECK(stay.phase() == Phase::kArmed);                                          // ignored in stay
  stay.set_zone("window", ZoneState::kTriggered, 14'000);
  CHECK(stay.phase() == Phase::kAlarm);
  // the exit delay does not alarm for the person leaving; the entry delay does when nobody disarms
  Panel leaving(timing(), house(), "1234");
  leaving.arm(Mode::kAway, false, 0);
  leaving.set_zone("front-door", ZoneState::kTriggered, 1000);
  leaving.set_zone("window", ZoneState::kTriggered, 2000);
  CHECK(leaving.phase() == Phase::kExitDelay);
  leaving.tick(10'000);
  CHECK(leaving.phase() == Phase::kAlarm);                                       // both were already open when the delay ended: the window sounds at once
  Panel coming(timing(), house(), "1234");
  coming.arm(Mode::kAway, false, 0);
  coming.tick(10'000);
  coming.set_zone("front-door", ZoneState::kTriggered, 20'000);
  CHECK(coming.phase() == Phase::kEntryDelay);
  coming.tick(29'999);
  CHECK(coming.phase() == Phase::kEntryDelay);
  coming.tick(30'000);
  CHECK(coming.phase() == Phase::kAlarm && coming.siren());
  coming.tick(30'000 + 59'999);
  CHECK(coming.siren());
  coming.tick(30'000 + 60'000);
  CHECK(!coming.siren() && coming.phase() == Phase::kAlarm);                    // the siren stops by itself; the alarm stays latched
  CHECK(coming.events().back().kind == EventKind::kSirenTimedOut);
  // a zone that has not sounded yet sounds the siren again, at most three times
  Panel again(timing(), house(), "1234");
  again.arm(Mode::kAway, false, 0);
  again.tick(10'000);
  again.set_zone("window", ZoneState::kTriggered, 11'000);
  again.tick(11'000 + 60'000);
  CHECK(!again.siren());
  again.set_zone("front-door", ZoneState::kTriggered, 80'000);
  CHECK(again.siren());
  // a zero delay arms at once and an entry zone with no entry delay alarms at once
  Timing none = timing();
  none.exit_delay_s = 0; none.entry_delay_s = 0;
  Panel instant(none, house(), "1234");
  CHECK(instant.arm(Mode::kAway, false, 0) == Refusal::kNone && instant.phase() == Phase::kArmed);
  instant.set_zone("front-door", ZoneState::kTriggered, 1);
  CHECK(instant.phase() == Phase::kAlarm);
}

// ---- the record a restart brings back --------------------------------------------------------------------------------------------------------------

static void test_saved_state() {
  for (const Phase phase : {Phase::kDisarmed, Phase::kArmed, Phase::kAlarm}) {
    for (const Mode mode : {Mode::kDisarmed, Mode::kAway, Mode::kStay}) {
      for (const bool silenced : {false, true}) {
        for (const std::uint32_t lockout : {0u, 1u, 2u, 3u}) {
          Panel::Saved saved;
          saved.phase = phase; saved.mode = mode; saved.silenced = silenced; saved.lockout = lockout;
          saved.bypassed = {"window", "hall"};
          Panel::Saved back;
          const std::string text = saved_text(saved);
          const bool consistent = (phase == Phase::kDisarmed) == (mode == Mode::kDisarmed) || phase == Phase::kAlarm;
          // what is written is read back whole, unless it is a state no panel is ever in
          CHECK(parse_saved(text, back) == consistent);
          if (consistent) CHECK(back == saved);
        }
      }
    }
  }
  Panel::Saved back;
  const char* refused[] = {"", "armed", "armed|away", "armed|away||x|y", "armed|away||silenced|extra", "ARMED|away|", "armed|AWAY|", "armed|sideways|", "sleeping|away|", "armed|disarmed|", "disarmed|away|",
                           "armed|away|window|lock4", "armed|away|window|lock0", "armed|away|window|silenced,wat", "armed|away|window|lock", "||", "|armed|away|", "armed|away|window|silenced|lock1"};
  for (const char* text : refused) CHECK(!parse_saved(text, back));
  CHECK(!parse_saved("armed|away|" + std::string(400, 'x'), back));
  CHECK(parse_saved("armed|away|window|", back) && back.bypassed.size() == 1 && !back.silenced && back.lockout == 0);   // an empty list of extras is no extras
  // a record cut short anywhere is either refused or a state a panel can be in: a damaged record never arms the panel at random
  const std::string whole = "alarm|stay|window,hall|silenced,lock2";
  CHECK(parse_saved(whole, back) && back.phase == Phase::kAlarm && back.mode == Mode::kStay && back.bypassed.size() == 2 && back.silenced && back.lockout == 2);
  for (std::size_t length = 0; length < whole.size(); ++length) {
    Panel::Saved piece;
    if (parse_saved(whole.substr(0, length), piece)) CHECK((piece.phase == Phase::kDisarmed) == (piece.mode == Mode::kDisarmed) || piece.phase == Phase::kAlarm);
  }
  // the pieces of a list: empty names are dropped
  CHECK(parse_saved("armed|away|,window,,hall,|", back) && back.bypassed.size() == 2);
  // restoring: an armed record arms; a disarmed one does nothing; the exit delay is not repeated
  Panel panel(timing(), house(), "1234");
  Panel::Saved armed;
  armed.phase = Phase::kArmed; armed.mode = Mode::kAway; armed.bypassed = {"window", "nobody"};
  CHECK(panel.restore(armed, 5000) && panel.phase() == Phase::kArmed && panel.mode() == Mode::kAway && panel.bypassed("window") && !panel.bypassed("hall"));
  CHECK(!panel.restore(armed, 6000));                                            // only into a disarmed panel
  Panel idle(timing(), house(), "1234");
  CHECK(!idle.restore(Panel::Saved{}, 0) && idle.phase() == Phase::kDisarmed);
  Panel::Saved odd;
  odd.phase = Phase::kArmed; odd.mode = Mode::kDisarmed;
  CHECK(!idle.restore(odd, 0) && idle.phase() == Phase::kDisarmed);
  // an alarm comes back as an alarm, a silenced one with its siren quiet, and an always-zone alarm with the system disarmed comes back too
  Panel ringing(timing(), house(), "1234");
  Panel::Saved alarm;
  alarm.phase = Phase::kAlarm; alarm.mode = Mode::kAway;
  CHECK(ringing.restore(alarm, 0) && ringing.phase() == Phase::kAlarm && ringing.siren());
  Panel quiet(timing(), house(), "1234");
  alarm.silenced = true;
  CHECK(quiet.restore(alarm, 0) && quiet.phase() == Phase::kAlarm && !quiet.siren());
  Panel smoke(timing(), house(), "1234");
  Panel::Saved fire;
  fire.phase = Phase::kAlarm; fire.mode = Mode::kDisarmed;
  CHECK(smoke.restore(fire, 0) && smoke.phase() == Phase::kAlarm && smoke.mode() == Mode::kDisarmed);
  // what the panel saves is what it takes back
  Panel source(timing(), house(), "1234");
  source.set_zone("window", ZoneState::kTrouble, 0);
  source.arm(Mode::kStay, true, 1);
  source.tick(11'000);
  source.set_zone("window", ZoneState::kNormal, 11'500);
  const Panel::Saved kept = source.saved();
  CHECK(kept.phase == Phase::kArmed && kept.mode == Mode::kStay && kept.bypassed.size() == 1 && kept.bypassed[0] == "window");
  Panel::Saved read;
  CHECK(parse_saved(saved_text(kept), read) && read == kept);
  // an entry delay that was running comes back as an alarm
  Panel entering(timing(), house(), "1234");
  entering.arm(Mode::kAway, false, 0);
  entering.tick(10'000);
  entering.set_zone("front-door", ZoneState::kTriggered, 11'000);
  CHECK(entering.phase() == Phase::kEntryDelay && entering.saved().phase == Phase::kAlarm);
}

static void test_lockout_survives_a_restart() {
  Panel panel(timing(), house(), "1234");
  for (const std::uint32_t level : {1u, 2u, 3u}) {
    Panel fresh(timing(), house(), "1234");
    Panel::Saved saved;
    saved.lockout = level;
    CHECK(!fresh.restore(saved, 1000));
    const std::uint64_t seconds = level == 1 ? 60 : level == 2 ? 300 : 900;
    CHECK(fresh.locked_out(1000 + seconds * 1000 - 1) && !fresh.locked_out(1000 + seconds * 1000));
  }
  Panel::Saved nonsense;
  nonsense.lockout = 4;
  Panel unlocked(timing(), house(), "1234");
  unlocked.restore(nonsense, 0);
  CHECK(!unlocked.locked_out(0));
  // the lockout a panel is in is the lockout it saves
  for (std::uint32_t round = 1; round <= 2; ++round) {
    for (int i = 0; i < 3; ++i) panel.disarm("0000", 1000 + round);
    CHECK(panel.saved().lockout == round);
    panel.tick(1'000'000 * round);
  }
  // the third lockout lasts fifteen times as long, and a restart during it brings back the same
  for (int i = 0; i < 3; ++i) panel.disarm("0000", 5'000'000);
  CHECK(panel.locked_out(5'000'000 + 899'999));
  CHECK(panel.saved().lockout == 3);
}

static void test_events_are_bounded_and_aged() {
  Timing patient = timing();
  patient.lockout_after = 100000;
  Panel panel(patient, house(), "1234");
  panel.set_epoch(1'700'000'000);
  CHECK(panel.epoch() == 1'700'000'000);
  panel.arm(Mode::kAway, false, 0);
  CHECK(panel.events().back().epoch == 1'700'000'000 && !panel.events().back().restored);
  for (int i = 0; i < 300; ++i) panel.disarm("bad", static_cast<std::uint64_t>(i) * 1000);
  CHECK(panel.events().size() == Panel::kMaxEvents);
  CHECK(panel.events().back().at_ms == 299'000);                                 // the newest are kept
  std::vector<Event> older(150);
  for (std::size_t i = 0; i < older.size(); ++i) { older[i].epoch = 1000 + static_cast<std::int64_t>(i); older[i].kind = EventKind::kArmed; }
  Panel fresh(timing(), house(), "1234");
  fresh.restore_events(older);
  CHECK(fresh.events().size() == Panel::kMaxEvents && fresh.events().front().epoch == 1050 && fresh.events().back().epoch == 1149 && fresh.events().front().restored);
  fresh.arm(Mode::kAway, false, 5);
  CHECK(!fresh.events().back().restored && fresh.events().size() == Panel::kMaxEvents);   // the ones of this run go after the ones brought back
  // the message tells an old event by its age on the wall clock and says nothing of one whose age is unknown
  Panel aged(timing(), house(), "1234");
  std::vector<Event> two(2);
  two[0].epoch = 1'700'000'000; two[0].kind = EventKind::kAlarm; two[0].zone = "window";
  two[1].epoch = 0; two[1].kind = EventKind::kDisarmed;
  aged.restore_events(two);
  const std::vector<ZoneReport> none;
  json::Value state = parsed(state_json("alarm-1", 1000, 5000, aged, false, false, none));
  CHECK(state.get("events")->items.empty());                                      // no real time yet: no age to tell
  aged.set_epoch(1'700'000'090);
  state = parsed(state_json("alarm-1", 1000, 5000, aged, false, false, none));
  CHECK(state.get("events")->items.size() == 1 && state.get("events")->items[0].number_or("ago_s", -1) == 90 && state.get("events")->items[0].string_or("zone", "") == "window");
  aged.set_epoch(1'600'000'000);                                                  // a clock that went backwards tells nothing
  state = parsed(state_json("alarm-1", 1000, 5000, aged, false, false, none));
  CHECK(state.get("events")->items.empty());
}

// ---- events in flash -----------------------------------------------------------------------------------------------------------------------------

static void test_events_text() {
  const EventKind kinds[] = {EventKind::kArmed, EventKind::kExitDelayStarted, EventKind::kEntryDelayStarted, EventKind::kAlarm, EventKind::kSirenTimedOut, EventKind::kDisarmed, EventKind::kBadPin,
                             EventKind::kLockedOut, EventKind::kZoneBypassed, EventKind::kSirenSilenced};
  std::set<std::string> names;
  for (const EventKind kind : kinds) names.insert(event_text(kind));
  CHECK(names.size() == 10 && names.count("siren_silenced") == 1);
  CHECK(std::string(mode_text(Mode::kAway)) == "away" && std::string(mode_text(Mode::kStay)) == "stay" && std::string(mode_text(Mode::kDisarmed)) == "disarmed");
  // each kind that is written is read back
  for (const EventKind kind : kinds) {
    std::vector<Event> one(1);
    one[0].epoch = 1'700'000'000; one[0].kind = kind; one[0].zone = "window";
    const std::vector<Event> back = parse_events(events_text(one));
    CHECK(back.size() == 1 && back[0].kind == kind && back[0].epoch == 1'700'000'000 && back[0].zone == "window");
  }
  // an event without real time is not kept, the newest ten are, in order
  std::vector<Event> many(15);
  for (std::size_t i = 0; i < many.size(); ++i) { many[i].epoch = 1000 + static_cast<std::int64_t>(i); many[i].kind = EventKind::kArmed; }
  many[14].epoch = 0;
  const std::vector<Event> kept = parse_events(events_text(many));
  CHECK(kept.size() == kSavedEvents && kept.front().epoch == 1004 && kept.back().epoch == 1013);
  CHECK(events_text({}).empty() && parse_events("").empty());
  // damaged text gives what can be read and never throws
  CHECK(parse_events("garbage").empty() && parse_events(";;;").empty() && parse_events("12,armed").empty() && parse_events(",armed,x").empty() && parse_events("12a,armed,").empty());
  CHECK(parse_events("1234567890123,armed,").empty());                            // more digits than any time has
  CHECK(parse_events("12,nonsense,").empty() && parse_events("12,armed,").size() == 1 && parse_events("12,armed,;13,alarm,window").size() == 2);
  CHECK(parse_events("12,armed," + std::string(33, 'z')).empty() && parse_events("12,armed," + std::string(32, 'z')).size() == 1);
  CHECK(parse_events("99999999999,armed,").size() == 1 && parse_events("99999999999,armed,")[0].epoch == 99999999999LL);
}

// ---- profiles and commands ----------------------------------------------------------------------------------------------------------------------------

static bool profile_ok(const std::string& text) { ArmProfile profile; return parse_profile_text(text, profile); }

static void test_profiles_are_read_strictly() {
  const std::string base = "{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\"";
  CHECK(profile_ok(base + "}"));
  CHECK(profile_ok(base + ",\"exit_delay_s\":0,\"entry_delay_s\":300,\"siren\":false,\"notify\":\"normal\",\"zones\":[{\"id\":\"hall\",\"armed\":true},{\"id\":\"window\",\"delay\":\"entry\"},{\"id\":\"x\",\"armed\":false}]}"));
  ArmProfile profile;
  CHECK(parse_profile_text(base + ",\"exit_delay_s\":5,\"siren\":false,\"notify\":\"normal\"}", profile) && profile.stay && profile.exit_delay_s == 5 && profile.entry_delay_s == -1 && !profile.siren && !profile.notify_urgent);
  const char* refused[] = {"", "[]", "{}", "{\"id\":\"night\",\"name\":\"Night\"}", "{\"id\":\"night\",\"base\":\"stay\"}", "{\"name\":\"Night\",\"base\":\"stay\"}"};
  for (const char* text : refused) CHECK(!profile_ok(text));
  CHECK(!profile_ok(base + ",\"extra\":1}") && !profile_ok(base + ",\"base\":\"away\"}"));
  CHECK(!profile_ok("{\"id\":\"Night\",\"name\":\"Night\",\"base\":\"stay\"}") && !profile_ok("{\"id\":\"\",\"name\":\"Night\",\"base\":\"stay\"}") && !profile_ok("{\"id\":\"" + std::string(17, 'a') + "\",\"name\":\"N\",\"base\":\"stay\"}") &&
        !profile_ok("{\"id\":\"-a\",\"name\":\"N\",\"base\":\"stay\"}") && profile_ok("{\"id\":\"" + std::string(16, 'a') + "\",\"name\":\"N\",\"base\":\"stay\"}"));
  CHECK(!profile_ok("{\"id\":\"n\",\"name\":\"\",\"base\":\"stay\"}") && !profile_ok("{\"id\":\"n\",\"name\":\"" + std::string(33, 'x') + "\",\"base\":\"stay\"}") && profile_ok("{\"id\":\"n\",\"name\":\"" + std::string(32, 'x') + "\",\"base\":\"stay\"}"));
  { std::string accents; for (int i = 0; i < 32; ++i) accents += "\xC3\xA9"; CHECK(utf8_length(accents) == 32 && profile_ok("{\"id\":\"n\",\"name\":\"" + accents + "\",\"base\":\"stay\"}") && !profile_ok("{\"id\":\"n\",\"name\":\"" + accents + "x\",\"base\":\"stay\"}")); }
  { std::string wide; for (int i = 0; i < 32; ++i) wide += "\xE2\x82\xAC"; CHECK(wide.size() == 96 && profile_ok("{\"id\":\"n\",\"name\":\"" + wide + "\",\"base\":\"stay\"}")); }
  { std::string wide; for (int i = 0; i < 32; ++i) wide += "\xF0\x9F\x98\x80"; CHECK(wide.size() == 128 && profile_ok("{\"id\":\"n\",\"name\":\"" + wide + "\",\"base\":\"stay\"}")); }
  CHECK(!profile_ok("{\"id\":\"n\",\"name\":\"N\",\"base\":\"sideways\"}") && !profile_ok("{\"id\":\"n\",\"name\":\"N\",\"base\":5}") && !profile_ok("{\"id\":5,\"name\":\"N\",\"base\":\"stay\"}"));
  // the delays: whole seconds, 0 to 300
  for (const char* key : {"exit_delay_s", "entry_delay_s"}) {
    for (const char* value : {"0", "1", "300", "1e2", "1.0"}) CHECK(profile_ok(base + ",\"" + key + "\":" + value + "}"));
    for (const char* value : {"-1", "301", "1.5", "\"5\"", "true", "null", "1e300", "-1e300", "99999999999999999999"}) CHECK(!profile_ok(base + ",\"" + key + "\":" + value + "}"));
  }
  CHECK(!profile_ok(base + ",\"siren\":1}") && !profile_ok(base + ",\"siren\":\"false\"}") && !profile_ok(base + ",\"notify\":\"loud\"}") && !profile_ok(base + ",\"notify\":true}"));
  // the zones: at most sixteen, each named once, a zone left out has no delay
  CHECK(!profile_ok(base + ",\"zones\":{}}") && !profile_ok(base + ",\"zones\":[1]}") && !profile_ok(base + ",\"zones\":[{}]}") && !profile_ok(base + ",\"zones\":[{\"id\":\"Bad Id\"}]}"));
  CHECK(!profile_ok(base + ",\"zones\":[{\"id\":\"a\"},{\"id\":\"a\"}]}") && !profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"extra\":1}]}"));
  CHECK(!profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"armed\":false,\"delay\":\"instant\"}]}") && !profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"armed\":false,\"delay\":\"entry\"}]}") &&
        profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"armed\":false}]}") && profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"armed\":true,\"delay\":\"entry\"}]}"));
  CHECK(!profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"armed\":1}]}") && !profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"delay\":\"later\"}]}") && !profile_ok(base + ",\"zones\":[{\"id\":\"a\",\"delay\":true}]}"));
  { std::string sixteen = ","; sixteen = ",\"zones\":["; for (int i = 0; i < 16; ++i) sixteen += std::string(i ? "," : "") + "{\"id\":\"z" + std::to_string(i) + "\"}"; CHECK(profile_ok(base + sixteen + "]}") && !profile_ok(base + sixteen + ",{\"id\":\"z16\"}]}")); }
  CHECK(!parse_profile_text(std::string(2049, ' ') + base + "}", profile));
  // what is written is read back the same
  ArmProfile full;
  full.id = "away-quiet"; full.name = "Away, quiet"; full.stay = false; full.exit_delay_s = 20; full.entry_delay_s = 0; full.siren = false; full.notify_urgent = false;
  full.zones = {{"hall", ArmedOverride::kNo, DelayOverride::kKind}, {"window", ArmedOverride::kKind, DelayOverride::kEntry}, {"front-door", ArmedOverride::kYes, DelayOverride::kInstant}};
  ArmProfile back;
  CHECK(parse_profile_text(profile_json(full), back) && profile_json(back) == profile_json(full) && back.zones.size() == 3 && back.zones[0].armed == ArmedOverride::kNo && back.zones[2].delay == DelayOverride::kInstant);
  ArmProfile plain;
  plain.id = "plain"; plain.name = "Plain";
  CHECK(profile_json(plain).find("exit_delay_s") == std::string::npos && profile_json(plain).find("zones") == std::string::npos && parse_profile_text(profile_json(plain), back) && back.exit_delay_s == -1);
  // what a zone does under a profile; an always zone ignores every override
  CHECK(resolve(ZoneKind::kAlways, true, &full, "hall").armed && !resolve(ZoneKind::kAlways, true, &full, "hall").delayed);
  ArmProfile quiet;
  quiet.zones = {{"smoke", ArmedOverride::kNo, DelayOverride::kKind}};
  CHECK(resolve(ZoneKind::kAlways, false, &quiet, "smoke").armed);
  CHECK(resolve(ZoneKind::kInterior, true, nullptr, "x").armed == false && resolve(ZoneKind::kInterior, false, nullptr, "x").armed && resolve(ZoneKind::kEntry, false, nullptr, "x").delayed && !resolve(ZoneKind::kInstant, false, nullptr, "x").delayed);
  CHECK(!resolve(ZoneKind::kInterior, false, &full, "hall").armed && resolve(ZoneKind::kInstant, false, &full, "window").delayed && !resolve(ZoneKind::kEntry, false, &full, "front-door").delayed && resolve(ZoneKind::kInterior, true, &full, "front-door").armed);
  CHECK(valid_profile_id("a") && valid_profile_id("0-_") && !valid_profile_id("_a") && !valid_profile_id("A"));
  CHECK(utf8_length("") == 0 && utf8_length("abc") == 3 && utf8_length("\xC3\xA9") == 1 && utf8_length("\xF0\x9F\x98\x80") == 1);
}

static std::string command(const std::string& body, const std::string& node = "alarm-1", const std::string& id = "0123456789abcdef") {
  return "{\"kind\":\"alarm_command\",\"node_id\":\"" + node + "\",\"timestamp_ms\":1000,\"command_id\":\"" + id + "\"," + body + "}";
}

static void test_commands_are_read_strictly() {
  Command out;
  CHECK(parse_command(command("\"action\":\"arm\",\"mode\":\"away\""), "alarm-1", out) && out.action == Action::kArm && out.mode == Mode::kAway && !out.force && !out.has_profile);
  CHECK(parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"force\":true"), "alarm-1", out) && out.mode == Mode::kStay && out.force);
  CHECK(parse_command(command("\"action\":\"disarm\""), "alarm-1", out) && out.action == Action::kDisarm && parse_command(command("\"action\":\"silence\""), "alarm-1", out) && out.action == Action::kSilence);
  const char* refused[] = {"\"action\":\"arm\"", "\"action\":\"arm\",\"mode\":\"night\"", "\"action\":\"arm\",\"mode\":5", "\"action\":\"disarm\",\"mode\":\"away\"", "\"action\":\"disarm\",\"force\":true",
                           "\"action\":\"silence\",\"mode\":\"stay\"", "\"action\":\"silence\",\"force\":true", "\"action\":\"disarm\",\"profile\":{}", "\"action\":\"explode\"", "\"action\":5", "\"mode\":\"away\"", "",
                           "\"action\":\"arm\",\"mode\":\"away\",\"force\":1", "\"action\":\"arm\",\"mode\":\"away\",\"extra\":1", "\"action\":\"ARM\",\"mode\":\"away\""};
  for (const char* body : refused) CHECK(!parse_command(command(body), "alarm-1", out));
  CHECK(!parse_command(command("\"action\":\"disarm\"", "alarm-2"), "alarm-1", out));                                  // for another node
  for (const char* id : {"", "0123456789abcde", "0123456789abcdef0", "0123456789ABCDEF", "0123456789abcdeg"}) CHECK(!parse_command(command("\"action\":\"disarm\"", "alarm-1", id), "alarm-1", out));
  CHECK(!parse_command("", "alarm-1", out) && !parse_command("[]", "alarm-1", out) && !parse_command("{", "alarm-1", out));
  std::string wrong_kind = command("\"action\":\"disarm\"");
  wrong_kind.replace(wrong_kind.find("alarm_command"), 13, "alarm_cmmand_");
  CHECK(!parse_command(wrong_kind, "alarm-1", out));
  for (const char* stamp : {"-1", "\"1\"", "null", "true"}) {
    std::string text = command("\"action\":\"disarm\"");
    text.replace(text.find("1000"), 4, stamp);
    CHECK(!parse_command(text, "alarm-1", out));
  }
  // a profile in an arm: its base is the mode of the arm, and it is read as strictly as it is alone
  const std::string night = "{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\"}";
  CHECK(parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":" + night), "alarm-1", out) && out.has_profile && out.profile.id == "night" && out.profile.stay);
  CHECK(!parse_command(command("\"action\":\"arm\",\"mode\":\"away\",\"profile\":" + night), "alarm-1", out));          // its base is not this arm's
  CHECK(!parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":{\"id\":\"n\"}"), "alarm-1", out) && !parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":5"), "alarm-1", out));
  // the answer repeats what was asked
  Command arm;
  parse_command(command("\"action\":\"arm\",\"mode\":\"away\""), "alarm-1", arm);
  const json::Value answer = parsed(result_json("alarm-1", 4321, arm, Refusal::kZonesOpen, Phase::kDisarmed));
  CHECK(answer.string_or("kind", "") == "alarm_result" && answer.string_or("command_id", "") == "0123456789abcdef" && answer.string_or("action", "") == "arm" && !answer.bool_or("accepted", true) &&
        answer.string_or("refusal", "") == "zones_open" && answer.string_or("phase", "") == "disarmed" && answer.number_or("timestamp_ms", 0) == 4321);
  CHECK(parsed(result_json("alarm-1", 1, arm, Refusal::kNone, Phase::kExitDelay)).bool_or("accepted", false) && std::string(refusal_text(Refusal::kLockedOut)) == "locked_out" && std::string(refusal_text(Refusal::kNotDisarmed)) == "not_disarmed");
  CHECK(valid_command_id("0123456789abcdef") && !valid_command_id("") && !valid_command_id("0123456789abcdeF"));
  // what a Zigbee zone says
  ZoneFeed feed;
  CHECK(parse_zone_feed("{\"zone\":\"window\",\"state\":\"triggered\",\"battery_low\":true}", feed) && feed.zone_id == "window" && feed.state == ZoneState::kTriggered && feed.battery_low);
  CHECK(parse_zone_feed("{\"zone\":\"window\",\"state\":\"normal\"}", feed) && !feed.battery_low && parse_zone_feed("{\"zone\":\"window\",\"state\":\"tamper\"}", feed) && feed.state == ZoneState::kTamper);
  for (const char* text : {"", "[]", "{}", "{\"zone\":\"w\"}", "{\"state\":\"normal\"}", "{\"zone\":\"\",\"state\":\"normal\"}", "{\"zone\":\"w\",\"state\":\"trouble\"}", "{\"zone\":\"w\",\"state\":\"open\"}",
                           "{\"zone\":\"w\",\"state\":5}", "{\"zone\":5,\"state\":\"normal\"}", "{\"zone\":\"w\",\"state\":\"normal\",\"battery_low\":1}", "{\"zone\":\"w\",\"state\":\"normal\",\"extra\":1}"}) CHECK(!parse_zone_feed(text, feed));
  CHECK(!parse_zone_feed("{\"zone\":\"" + std::string(33, 'z') + "\",\"state\":\"normal\"}", feed));
  CHECK(topic("alarm-1", "state") == "armor/alarm/alarm-1/state" && topic("Bad Node", "state").empty() && topic("", "command").empty());
}

// ---- the wired loops ---------------------------------------------------------------------------------------------------------------------------------------

static void test_loops_and_contacts() {
  const LoopLimits limits;
  CHECK(classify_loop(0, limits) == ZoneState::kTamper && classify_loop(399.9, limits) == ZoneState::kTamper && classify_loop(400, limits) == ZoneState::kNormal && classify_loop(1199.9, limits) == ZoneState::kNormal &&
        classify_loop(1200, limits) == ZoneState::kTriggered && classify_loop(2399.9, limits) == ZoneState::kTriggered && classify_loop(2400, limits) == ZoneState::kTamper && classify_loop(3300, limits) == ZoneState::kTamper);
  CHECK(classify_loop(-5, limits) == ZoneState::kTamper);
  CHECK(classify_loop(std::nan(""), limits) == ZoneState::kTamper);              // a reading that is not a number is a loop that cannot be trusted, never a quiet one
  CHECK(limits_are_valid(limits));
  LoopLimits bad = limits;
  bad.short_below_mv = 0; CHECK(!limits_are_valid(bad));
  bad = limits; bad.normal_below_mv = bad.short_below_mv; CHECK(!limits_are_valid(bad));
  bad = limits; bad.triggered_below_mv = bad.normal_below_mv; CHECK(!limits_are_valid(bad));
  bad = limits; bad.triggered_below_mv = 3000; CHECK(limits_are_valid(bad));
  bad.triggered_below_mv = 3001; CHECK(!limits_are_valid(bad));
  bad = limits; bad.short_below_mv = -1; CHECK(!limits_are_valid(bad));
  bad = limits; bad.short_below_mv = std::nan(""); CHECK(!limits_are_valid(bad));
  // a contact that is closed when all is well is triggered when it opens; a tamper switch says tamper
  CHECK(classify_contact(false, true, false) == ZoneState::kNormal && classify_contact(true, true, false) == ZoneState::kTriggered && classify_contact(true, true, true) == ZoneState::kTamper &&
        classify_contact(false, true, true) == ZoneState::kNormal);
  CHECK(classify_contact(true, false, false) == ZoneState::kNormal && classify_contact(false, false, false) == ZoneState::kTriggered && classify_contact(false, false, true) == ZoneState::kTamper);
  CHECK(valid_zone_id("a") && valid_zone_id("front-door_1") && valid_zone_id(std::string(32, 'a')) && !valid_zone_id("") && !valid_zone_id(std::string(33, 'a')) && !valid_zone_id("-a") && !valid_zone_id("_a") && !valid_zone_id("A") && !valid_zone_id("a b"));
  ZoneKind kind = ZoneKind::kInstant;
  for (const ZoneKind each : {ZoneKind::kInstant, ZoneKind::kEntry, ZoneKind::kInterior, ZoneKind::kAlways}) CHECK(parse_kind(kind_text(each), kind) && kind == each);
  CHECK(!parse_kind("", kind) && !parse_kind("Instant", kind) && !parse_kind("24h", kind));
  CHECK(std::string(state_text(ZoneState::kNormal)) == "normal" && std::string(state_text(ZoneState::kTriggered)) == "triggered" && std::string(state_text(ZoneState::kTamper)) == "tamper" && std::string(state_text(ZoneState::kTrouble)) == "trouble");
  // a button fires once per press, after it has held; a button that bounces fires once
  ButtonFilter button(50);
  CHECK(!button.feed(true, 0) && !button.feed(true, 49) && button.feed(true, 50) && !button.feed(true, 500));
  CHECK(!button.feed(false, 600) && !button.feed(true, 700) && !button.feed(false, 720) && !button.feed(true, 730) && !button.feed(true, 779) && button.feed(true, 780));   // a bounce restarts the hold
  // a reading must hold; the first one holds too, starting from normal
  Debouncer debouncer(200);
  CHECK(debouncer.feed(ZoneState::kTamper, 0) == ZoneState::kNormal && debouncer.feed(ZoneState::kTamper, 199) == ZoneState::kNormal && debouncer.feed(ZoneState::kTamper, 200) == ZoneState::kTamper);
  CHECK(debouncer.feed(ZoneState::kNormal, 300) == ZoneState::kTamper && debouncer.feed(ZoneState::kTamper, 350) == ZoneState::kTamper && debouncer.feed(ZoneState::kNormal, 400) == ZoneState::kTamper &&
        debouncer.feed(ZoneState::kNormal, 599) == ZoneState::kTamper && debouncer.feed(ZoneState::kNormal, 600) == ZoneState::kNormal && debouncer.state() == ZoneState::kNormal);
}

int main() {
  test_lockout_grows_and_forgives();
  test_arming_rules();
  test_saved_state();
  test_lockout_survives_a_restart();
  test_events_are_bounded_and_aged();
  test_events_text();
  test_profiles_are_read_strictly();
  test_commands_are_read_strictly();
  test_loops_and_contacts();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
