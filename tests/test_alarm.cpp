// ARMOR-NODE-ALARM - host tests of the zones and the panel: scripted scenarios, then a long random run that checks the rules that must always hold.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Nothing here has run on a real installation: the zones are stand-ins and the clock is a number the test moves.
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "../core/alarm_panel.hpp"
#include "../core/alarm_zone.hpp"

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

using namespace armor::alarm;

static std::vector<Zone> house() {
  return {{"front-door", "Front door", ZoneKind::kEntry}, {"window", "Living room window", ZoneKind::kInstant}, {"hall-motion", "Hall motion", ZoneKind::kInterior},
          {"smoke", "Kitchen smoke detector", ZoneKind::kAlways}};
}
static Timing quick() { Timing t; t.exit_delay_s = 10; t.entry_delay_s = 10; t.siren_max_s = 60; t.lockout_after = 3; t.lockout_s = 30; return t; }
static bool has_event(const Panel& p, EventKind kind) { for (const Event& e : p.events()) if (e.kind == kind) return true; return false; }

// ---- the loop ----------------------------------------------------------------------------------------------------------------------

static void test_loop() {
  const LoopLimits limits;   // 400 / 1200 / 2400 mV
  CHECK(limits_are_valid(limits));
  CHECK(classify_loop(0, limits) == ZoneState::kTamper && classify_loop(399, limits) == ZoneState::kTamper);      // shorted
  CHECK(classify_loop(400, limits) == ZoneState::kNormal && classify_loop(1199, limits) == ZoneState::kNormal);   // the closed contact
  CHECK(classify_loop(1200, limits) == ZoneState::kTriggered && classify_loop(2399, limits) == ZoneState::kTriggered);   // the open contact
  CHECK(classify_loop(2400, limits) == ZoneState::kTamper && classify_loop(3300, limits) == ZoneState::kTamper);   // a cut wire
  CHECK(limits_are_valid({400, 1200, 3000}) && !limits_are_valid({400, 1200, 3200}));   // the converter does not read above about 3.1 V: a limit up there would take a cut wire for a contact
  LoopLimits bad = limits;
  bad.short_below_mv = 0; CHECK(!limits_are_valid(bad)); bad = limits;
  bad.normal_below_mv = 300; CHECK(!limits_are_valid(bad)); bad = limits;
  bad.triggered_below_mv = 1200; CHECK(!limits_are_valid(bad)); bad = limits;
  bad.triggered_below_mv = 3400; CHECK(!limits_are_valid(bad));
  // every reading in range is some state, and the states come in order as the voltage rises
  int last = -1;
  for (int mv = 0; mv <= 3300; mv += 10) {
    const int order = classify_loop(mv, limits) == ZoneState::kNormal ? 1 : classify_loop(mv, limits) == ZoneState::kTriggered ? 2 : (mv < 800 ? 0 : 3);
    CHECK(order >= last); last = order;
  }
}

static void test_zone_names() {
  for (ZoneKind kind : {ZoneKind::kInstant, ZoneKind::kEntry, ZoneKind::kInterior, ZoneKind::kAlways}) { ZoneKind back = ZoneKind::kInstant; CHECK(parse_kind(kind_text(kind), back) && back == kind); }
  ZoneKind none; CHECK(!parse_kind("24h", none) && !parse_kind("", none));
  CHECK(valid_zone_id("front-door") && valid_zone_id("z1") && !valid_zone_id("") && !valid_zone_id("-x") && !valid_zone_id("Front") && !valid_zone_id("a b") && !valid_zone_id(std::string(33, 'a')));
  CHECK(std::string(state_text(ZoneState::kNormal)) == "normal" && std::string(state_text(ZoneState::kTriggered)) == "triggered" && std::string(state_text(ZoneState::kTamper)) == "tamper");
}

static void test_debounce() {
  Debouncer d(200);
  CHECK(d.feed(ZoneState::kNormal, 0) == ZoneState::kNormal);
  CHECK(d.feed(ZoneState::kTriggered, 100) == ZoneState::kNormal);        // a change must hold
  CHECK(d.feed(ZoneState::kNormal, 150) == ZoneState::kNormal);           // it bounced back: nothing happened
  CHECK(d.feed(ZoneState::kTriggered, 160) == ZoneState::kNormal);
  CHECK(d.feed(ZoneState::kTriggered, 300) == ZoneState::kNormal);        // 140 ms
  CHECK(d.feed(ZoneState::kTriggered, 360) == ZoneState::kTriggered);     // 200 ms
  CHECK(d.feed(ZoneState::kNormal, 400) == ZoneState::kTriggered);        // and so does the way back
  CHECK(d.feed(ZoneState::kNormal, 600) == ZoneState::kNormal);
  Debouncer first(500);
  CHECK(first.feed(ZoneState::kTriggered, 5) == ZoneState::kNormal);      // the first reading holds like any other (a converter still settling at power-up gave a false tamper)
  CHECK(first.feed(ZoneState::kTriggered, 300) == ZoneState::kNormal && first.feed(ZoneState::kTriggered, 505) == ZoneState::kTriggered);
}

// ---- arming and the delays ---------------------------------------------------------------------------------------------------------

static void test_arm_and_disarm() {
  Panel p(quick(), house(), "1234");
  CHECK(p.phase() == Phase::kDisarmed && p.mode() == Mode::kDisarmed && !p.siren());
  CHECK(p.arm(Mode::kAway, false, 1000) == Refusal::kNone);
  CHECK(p.phase() == Phase::kExitDelay && p.mode() == Mode::kAway && has_event(p, EventKind::kExitDelayStarted));
  p.tick(5000); CHECK(p.phase() == Phase::kExitDelay);
  p.tick(11000); CHECK(p.phase() == Phase::kArmed && has_event(p, EventKind::kArmed));
  CHECK(p.arm(Mode::kStay, false, 12000) == Refusal::kNotDisarmed);          // already armed
  CHECK(p.arm(Mode::kDisarmed, false, 12000) == Refusal::kNotArmed);
  CHECK(p.disarm("0000", 13000) == Refusal::kBadPin && p.phase() == Phase::kArmed);
  CHECK(p.disarm("1234", 14000) == Refusal::kNone && p.phase() == Phase::kDisarmed && p.mode() == Mode::kDisarmed && has_event(p, EventKind::kDisarmed));
  CHECK(p.disarm("1234", 15000) == Refusal::kNone && p.phase() == Phase::kDisarmed);   // disarming a disarmed panel is harmless
}

static void test_instant_zone() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  p.set_zone("window", ZoneState::kTriggered, 20000);
  CHECK(p.phase() == Phase::kAlarm && p.siren() && has_event(p, EventKind::kAlarm) && p.events().back().zone == "window");
  p.set_zone("window", ZoneState::kNormal, 21000);
  CHECK(p.phase() == Phase::kAlarm && p.siren());                            // the alarm latches: closing the window does not end it
  CHECK(p.disarm("9999", 22000) == Refusal::kBadPin && p.phase() == Phase::kAlarm && p.siren());
  CHECK(p.disarm("1234", 23000) == Refusal::kNone && !p.siren() && p.phase() == Phase::kDisarmed);
}

static void test_entry_delay() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  p.set_zone("front-door", ZoneState::kTriggered, 20000);
  CHECK(p.phase() == Phase::kEntryDelay && !p.siren() && has_event(p, EventKind::kEntryDelayStarted));
  p.tick(25000); CHECK(p.phase() == Phase::kEntryDelay);
  CHECK(p.disarm("1234", 26000) == Refusal::kNone && p.phase() == Phase::kDisarmed);   // in time: no alarm
  CHECK(!has_event(p, EventKind::kAlarm));

  Panel q(quick(), house(), "1234");
  q.arm(Mode::kAway, false, 0); q.tick(11000);
  q.set_zone("front-door", ZoneState::kTriggered, 20000);
  q.tick(29999); CHECK(q.phase() == Phase::kEntryDelay);
  q.tick(30000); CHECK(q.phase() == Phase::kAlarm && q.siren() && q.events().back().zone == "front-door");   // too late
  // an instant zone during the entry delay alarms at once (somebody went past the door into the window)
  Panel r(quick(), house(), "1234");
  r.arm(Mode::kAway, false, 0); r.tick(11000);
  r.set_zone("front-door", ZoneState::kTriggered, 20000);
  r.set_zone("window", ZoneState::kTriggered, 21000);
  CHECK(r.phase() == Phase::kAlarm);
}

static void test_exit_delay() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0);
  p.set_zone("front-door", ZoneState::kTriggered, 3000);    // leaving
  p.set_zone("hall-motion", ZoneState::kTriggered, 4000);
  p.set_zone("window", ZoneState::kTriggered, 5000);
  CHECK(p.phase() == Phase::kExitDelay && !p.siren());       // nobody is an intruder yet...
  p.tick(11000);
  CHECK(p.phase() == Phase::kAlarm);                         // ...but what is still open when the delay ends counts (the window), as if it had just opened
  Panel q(quick(), house(), "1234");
  q.arm(Mode::kAway, false, 0);
  q.set_zone("front-door", ZoneState::kTriggered, 3000);
  q.set_zone("front-door", ZoneState::kNormal, 6000);        // the door was closed behind the person
  q.tick(11000);
  CHECK(q.phase() == Phase::kArmed);
  Panel z(quick(), house(), "1234");
  z.arm(Mode::kAway, false, 0);
  z.set_zone("smoke", ZoneState::kTriggered, 2000);          // smoke in the exit delay
  CHECK(z.phase() == Phase::kAlarm && z.siren());
}

static void test_modes() {
  Panel stay(quick(), house(), "1234");
  stay.arm(Mode::kStay, false, 0); stay.tick(11000);
  stay.set_zone("hall-motion", ZoneState::kTriggered, 20000);
  CHECK(stay.phase() == Phase::kArmed);                      // inside, with people at home: ignored
  stay.set_zone("window", ZoneState::kTriggered, 21000);
  CHECK(stay.phase() == Phase::kAlarm);                      // the perimeter is not
  Panel away(quick(), house(), "1234");
  away.arm(Mode::kAway, false, 0); away.tick(11000);
  away.set_zone("hall-motion", ZoneState::kTriggered, 20000);
  CHECK(away.phase() == Phase::kAlarm);
}

static void test_always_zones_and_tamper() {
  Panel p(quick(), house(), "1234");                          // disarmed
  p.set_zone("smoke", ZoneState::kTriggered, 1000);
  CHECK(p.phase() == Phase::kAlarm && p.siren() && p.mode() == Mode::kDisarmed);
  CHECK(p.disarm("1234", 2000) == Refusal::kNone && p.phase() == Phase::kDisarmed);
  // still sounding after disarm: it does not restart on its own, only when it has been quiet and sounds again
  p.tick(3000); CHECK(p.phase() == Phase::kDisarmed);
  p.set_zone("smoke", ZoneState::kNormal, 4000);
  p.set_zone("smoke", ZoneState::kTriggered, 5000);
  CHECK(p.phase() == Phase::kAlarm);
  p.disarm("1234", 6000);
  // a tamper alarms in every state, on any kind of zone, even disarmed
  Panel t(quick(), house(), "1234");
  t.set_zone("hall-motion", ZoneState::kTamper, 1000);
  CHECK(t.phase() == Phase::kAlarm && t.events().back().zone == "hall-motion");
  // a zone that cannot be read never sounds, stops an arming, and can be left out like any other open zone
  Panel u(quick(), house(), "1234");
  u.set_zone("hall-motion", ZoneState::kTrouble, 100);
  CHECK(u.phase() == Phase::kDisarmed && u.zone_state("hall-motion") == ZoneState::kTrouble);
  CHECK(std::string(state_text(ZoneState::kTrouble)) == "trouble");
  CHECK(u.arm(Mode::kAway, false, 200) == Refusal::kZonesOpen);
  CHECK(u.arm(Mode::kAway, true, 300) == Refusal::kNone);       // forced: the zone is left out and the arming says so
  u.tick(300 + 120000);
  u.set_zone("hall-motion", ZoneState::kTrouble, 500000);
  CHECK(u.phase() != Phase::kAlarm);
  // an always-zone sounding cannot be armed over, forced or not
  Panel s(quick(), house(), "1234");
  s.set_zone("smoke", ZoneState::kTriggered, 0);
  s.disarm("1234", 100);
  CHECK(s.arm(Mode::kAway, false, 200) == Refusal::kZonesOpen && s.arm(Mode::kAway, true, 300) == Refusal::kZonesOpen && s.phase() == Phase::kDisarmed);
}

static void test_open_zones_at_arming() {
  Panel p(quick(), house(), "1234");
  p.set_zone("window", ZoneState::kTriggered, 0);
  CHECK(p.open_zones(Mode::kAway).size() == 1 && p.open_zones(Mode::kAway)[0] == "window");
  CHECK(p.arm(Mode::kAway, false, 100) == Refusal::kZonesOpen && p.phase() == Phase::kDisarmed);
  CHECK(p.arm(Mode::kAway, true, 200) == Refusal::kNone && p.bypassed("window") && has_event(p, EventKind::kZoneBypassed));
  p.tick(11000);
  CHECK(p.phase() == Phase::kArmed);                          // the open window is left out...
  p.set_zone("window", ZoneState::kNormal, 12000);
  p.set_zone("window", ZoneState::kTriggered, 13000);
  CHECK(p.phase() == Phase::kArmed);                          // ...for this arming
  p.disarm("1234", 14000);
  CHECK(!p.bypassed("window"));                               // and it counts again after the disarm
  p.set_zone("window", ZoneState::kNormal, 15000);
  CHECK(p.arm(Mode::kAway, false, 16000) == Refusal::kNone);
  // an interior zone that is open does not stop a "stay"
  Panel q(quick(), house(), "1234");
  q.set_zone("hall-motion", ZoneState::kTriggered, 0);
  CHECK(q.arm(Mode::kAway, false, 100) == Refusal::kZonesOpen && q.arm(Mode::kStay, false, 200) == Refusal::kNone);
  // an unknown zone is ignored
  q.set_zone("nope", ZoneState::kTriggered, 300); CHECK(!q.bypassed("nope") && q.zone_state("nope") == ZoneState::kNormal);
}

static void test_lockout_grows_until_a_right_pin() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 10000);
  // quick(): three wrong PINs lock for 30 s; the second lockout lasts five times as long and the third fifteen times
  for (int i = 0; i < 3; ++i) p.disarm("0000", 12000 + i);
  CHECK(p.locked_out(12000 + 29000) && !p.locked_out(12002 + 30000));
  for (int i = 0; i < 3; ++i) p.disarm("0000", 50000 + i);
  CHECK(p.locked_out(50002 + 149000) && !p.locked_out(50002 + 150000));
  for (int i = 0; i < 3; ++i) p.disarm("0000", 300000 + i);
  CHECK(p.locked_out(300002 + 449000) && !p.locked_out(300002 + 450000));
  for (int i = 0; i < 3; ++i) p.disarm("0000", 800000 + i);
  CHECK(p.locked_out(800002 + 449000) && !p.locked_out(800002 + 450000));   // it stops growing at fifteen times
  CHECK(p.disarm("1234", 800002 + 450000) == Refusal::kNone);              // the right PIN, and the next lockout is short again
  p.arm(Mode::kAway, false, 1300000);
  for (int i = 0; i < 3; ++i) p.disarm("0000", 1400000 + i);
  CHECK(!p.locked_out(1400002 + 30000));
}

static void test_pin_and_lockout() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  CHECK(p.disarm("1111", 12000) == Refusal::kBadPin && p.disarm("2222", 12100) == Refusal::kBadPin && !p.locked_out(12200));
  CHECK(p.disarm("3333", 12200) == Refusal::kBadPin && p.locked_out(12300) && has_event(p, EventKind::kLockedOut));
  CHECK(p.disarm("1234", 13000) == Refusal::kLockedOut && p.phase() == Phase::kArmed);   // even the right PIN, while locked out
  CHECK(p.arm(Mode::kAway, false, 13500) == Refusal::kNotDisarmed);   // arming is never held back by the lockout (it needs no PIN): here it is refused only because the panel is armed
  p.tick(43000); CHECK(!p.locked_out(43000));                                              // 30 s later
  CHECK(p.disarm("1234", 43500) == Refusal::kNone && p.phase() == Phase::kDisarmed);
  // a right PIN clears the count
  Panel q(quick(), house(), "1234");
  q.arm(Mode::kAway, false, 0); q.tick(11000);
  q.disarm("0", 12000); q.disarm("0", 12100); q.disarm("1234", 12200);
  q.arm(Mode::kAway, false, 12300); q.tick(30000);
  q.disarm("0", 31000); q.disarm("0", 31100);
  CHECK(!q.locked_out(31200));
  // PINs of another length, empty ones, and a panel with no PIN set never match
  CHECK(Panel(quick(), house(), "1234").disarm("", 0) == Refusal::kBadPin && Panel(quick(), house(), "1234").disarm("12345", 0) == Refusal::kBadPin && Panel(quick(), house(), "1234").disarm("123", 0) == Refusal::kBadPin);
  Panel none(quick(), house(), "");
  none.arm(Mode::kAway, false, 0); none.tick(11000);
  CHECK(none.disarm("", 12000) == Refusal::kBadPin && none.phase() == Phase::kArmed);
}

static void test_siren_times_out() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  p.set_zone("window", ZoneState::kTriggered, 20000);
  p.tick(79999); CHECK(p.siren());
  p.tick(80000); CHECK(!p.siren() && p.phase() == Phase::kAlarm && has_event(p, EventKind::kSirenTimedOut));   // quiet, but still an alarm
  CHECK(p.disarm("1234", 90000) == Refusal::kNone && p.phase() == Phase::kDisarmed);
}

static void test_silence_keeps_the_alarm() {
  Panel p(quick(), house(), "1234");
  CHECK(p.silence("1234", 0) == Refusal::kNotArmed);                       // nothing is ringing
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  CHECK(p.silence("1234", 12000) == Refusal::kNotArmed);                   // armed, quiet: nothing to silence
  p.set_zone("window", ZoneState::kTriggered, 20000);
  CHECK(p.siren() && p.phase() == Phase::kAlarm);
  CHECK(p.silence("0000", 21000) == Refusal::kBadPin && p.siren() && has_event(p, EventKind::kBadPin));   // a wrong PIN silences nothing
  CHECK(p.silence("1234", 22000) == Refusal::kNone);
  CHECK(!p.siren() && p.phase() == Phase::kAlarm && p.mode() == Mode::kAway && has_event(p, EventKind::kSirenSilenced));   // still armed and latched
  CHECK(p.saved().phase == Phase::kAlarm && p.saved().silenced);           // a restart does not make it louder
  const std::size_t events = p.events().size();
  CHECK(p.silence("1234", 23000) == Refusal::kNone && p.events().size() == events);   // already quiet: nothing new is noted
  // a zone that has not sounded yet sounds it again, as before
  p.set_zone("front-door", ZoneState::kTriggered, 30000);
  CHECK(p.siren() && p.phase() == Phase::kAlarm);
  CHECK(p.silence("1234", 31000) == Refusal::kNone && !p.siren());
  CHECK(p.disarm("1234", 32000) == Refusal::kNone && p.phase() == Phase::kDisarmed);
}

static void test_silence_counts_wrong_pins() {
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  p.set_zone("window", ZoneState::kTriggered, 20000);
  CHECK(p.silence("1", 21000) == Refusal::kBadPin && p.silence("2", 22000) == Refusal::kBadPin);
  CHECK(p.disarm("3", 23000) == Refusal::kBadPin && p.locked_out(23000));                  // silence and disarm count together
  CHECK(p.silence("1234", 24000) == Refusal::kLockedOut && p.siren());                      // locked out, even the right PIN
  CHECK(p.silence("1234", 23000 + 30000) == Refusal::kNone && !p.siren());
  // the server's say-so needs no PIN and no lockout to pass
  Panel q(quick(), house(), "1234");
  q.arm(Mode::kAway, false, 0); q.tick(11000);
  q.set_zone("window", ZoneState::kTriggered, 20000);
  for (int i = 0; i < 3; ++i) q.silence("0", 21000 + i);
  CHECK(q.locked_out(22000) && q.silence_authorized(22000) == Refusal::kNone && !q.siren() && q.phase() == Phase::kAlarm);
}

static void test_zero_delays_and_log() {
  Timing t = quick(); t.exit_delay_s = 0; t.entry_delay_s = 0;
  Panel p(t, house(), "1234");
  CHECK(p.arm(Mode::kAway, false, 0) == Refusal::kNone && p.phase() == Phase::kArmed);
  p.set_zone("front-door", ZoneState::kTriggered, 1000);
  CHECK(p.phase() == Phase::kAlarm);                                       // no entry delay: alarm at once
  // the log keeps the last hundred events
  Panel q(quick(), house(), "1234");
  for (int i = 0; i < 150; ++i) q.disarm("0", static_cast<std::uint64_t>(i) * 100000);   // each pair of wrong PINs... three lock out: plenty of events
  CHECK(q.events().size() <= Panel::kMaxEvents && !q.events().empty());
  CHECK(std::string(phase_text(Phase::kEntryDelay)) == "entry_delay" && std::string(refusal_text(Refusal::kZonesOpen)) == "zones_open" && std::string(refusal_text(Refusal::kLockedOut)) == "locked_out");
}

// ---- a long random run ---------------------------------------------------------------------------------------------------------------

// Whatever happens, these must always hold: a siren means an alarm; an alarm only ends with a right PIN; a disarmed panel is quiet unless an always-zone or a tamper made it ring; a wrong PIN
// never disarms; the entry delay never lets more than its time pass.
static void test_random() {
  std::mt19937 rng(20260);
  const std::vector<Zone> zones = house();
  const char* ids[] = {"front-door", "window", "hall-motion", "smoke", "nope"};
  for (int round = 0; round < 60; ++round) {
    Timing t; t.exit_delay_s = static_cast<std::uint32_t>(rng() % 20); t.entry_delay_s = static_cast<std::uint32_t>(rng() % 20); t.siren_max_s = 1 + static_cast<std::uint32_t>(rng() % 100);
    t.lockout_after = 1 + static_cast<std::uint32_t>(rng() % 4); t.lockout_s = 1 + static_cast<std::uint32_t>(rng() % 60);
    Panel p(t, zones, "4321");
    std::uint64_t now = 0;
    bool forced_alarm_source = false;   // an always-zone or a tamper was fed since the last disarm
    std::uint64_t entry_since = 0;
    bool in_entry = false;
    for (int step = 0; step < 3000; ++step) {
      now += rng() % 4000;
      const Phase before = p.phase();
      const bool was_siren = p.siren();
      bool zone_fed = false;   // a zone that sounds can bring a timed-out siren back; nothing else can
      switch (rng() % 7) {
        case 0: case 1: {
          const std::string id = ids[rng() % 5];
          const ZoneState state = static_cast<ZoneState>(rng() % 3);
          if (state == ZoneState::kTamper || id == "smoke") forced_alarm_source = forced_alarm_source || state != ZoneState::kNormal;
          p.set_zone(id, state, now);
          zone_fed = true;
          break;
        }
        case 2: p.arm(static_cast<Mode>(1 + rng() % 2), rng() % 2 == 0, now); break;
        case 3: {
          const bool right = rng() % 3 == 0;
          const Refusal r = p.disarm(right ? "4321" : "0000", now);
          if (!right) { CHECK(r == Refusal::kBadPin || r == Refusal::kLockedOut); CHECK(before == Phase::kDisarmed || p.phase() != Phase::kDisarmed); }   // a wrong PIN never disarms
          if (r == Refusal::kNone && right) forced_alarm_source = false;
          break;
        }
        default: p.tick(now); break;
      }
      p.tick(now);
      // a siren means an alarm
      CHECK(!p.siren() || p.phase() == Phase::kAlarm);
      // an alarm only ends through a disarm: leaving the alarm phase means disarmed
      if (before == Phase::kAlarm) CHECK(p.phase() == Phase::kAlarm || p.phase() == Phase::kDisarmed);
      // a siren that was off in an alarm does not come back on by itself, only a zone that sounds brings it back
      if (before == Phase::kAlarm && !was_siren && p.phase() == Phase::kAlarm && !zone_fed) CHECK(!p.siren());
      // a disarmed, quiet history of the always-zones and tampers means no alarm
      if (!forced_alarm_source && p.phase() == Phase::kDisarmed) CHECK(!p.siren());
      // the entry delay never runs past its time (ticked every step, so within one tick)
      if (p.phase() == Phase::kEntryDelay) { if (!in_entry) { in_entry = true; entry_since = now; } CHECK(now - entry_since <= static_cast<std::uint64_t>(t.entry_delay_s) * 1000ULL + 1); }
      else in_entry = false;
      // the exit delay and the lockout are consistent with the mode
      if (p.phase() == Phase::kDisarmed) CHECK(p.mode() == Mode::kDisarmed);
      if (p.phase() == Phase::kExitDelay || p.phase() == Phase::kArmed || p.phase() == Phase::kEntryDelay) CHECK(p.mode() != Mode::kDisarmed);
    }
  }
}

// ---- what a restart must not forget; sabotage that cannot be left out ----------------------------------------------------------------------

static void test_restart_and_tamper() {
  // armed away with a window left out, then the power goes: the panel comes back armed, with the same window left out
  Panel before(quick(), house(), "1234");
  before.set_zone("window", ZoneState::kTriggered, 0);
  CHECK(before.arm(Mode::kAway, true, 0) == Refusal::kNone);
  before.tick(11000);
  CHECK(before.phase() == Phase::kArmed && before.bypassed("window"));
  const Panel::Saved saved = before.saved();
  CHECK(saved.phase == Phase::kArmed && saved.mode == Mode::kAway && saved.bypassed.size() == 1 && saved.bypassed[0] == "window");
  Panel::Saved read;
  CHECK(parse_saved(saved_text(saved), read) && read == saved);
  Panel after(quick(), house(), "1234");
  CHECK(after.restore(read, 50));
  CHECK(after.phase() == Phase::kArmed && after.mode() == Mode::kAway && after.bypassed("window") && !after.siren());
  after.set_zone("front-door", ZoneState::kTriggered, 100);   // it guards: the entry delay starts
  CHECK(after.phase() == Phase::kEntryDelay);
  // an exit delay that was running comes back armed; an entry delay that was running comes back as an alarm; a sounding alarm sounds again
  Panel exiting(quick(), house(), "1234");
  CHECK(exiting.arm(Mode::kStay, false, 0) == Refusal::kNone && exiting.phase() == Phase::kExitDelay && exiting.saved().phase == Phase::kArmed);
  Panel entering(quick(), house(), "1234");
  entering.arm(Mode::kAway, false, 0); entering.tick(11000); entering.set_zone("front-door", ZoneState::kTriggered, 12000);
  CHECK(entering.phase() == Phase::kEntryDelay && entering.saved().phase == Phase::kAlarm);
  Panel again(quick(), house(), "1234");
  CHECK(again.restore(entering.saved(), 5) && again.phase() == Phase::kAlarm && again.siren());
  // a disarmed panel saves nothing and restores nothing; a record that is not exactly a saved state arms nothing
  Panel quiet(quick(), house(), "1234");
  CHECK(quiet.saved().phase == Phase::kDisarmed && !quiet.restore(quiet.saved(), 0) && quiet.phase() == Phase::kDisarmed);
  Panel::Saved junk;
  for (const char* text : {"", "armed", "armed|away", "armed|sideways|", "dancing|away|", "armed|disarmed|", "disarmed|away|", "armed|away|"})
    if (parse_saved(text, junk)) { Panel p(quick(), house(), "1234"); CHECK(!(junk.phase != Phase::kDisarmed && junk.mode == Mode::kDisarmed && p.restore(junk, 0))); }
  CHECK(!parse_saved("armed|sideways|", junk) && !parse_saved("armed|disarmed|", junk) && !parse_saved("disarmed|away|", junk) && !parse_saved("armed|away", junk));
  CHECK(parse_saved("alarm|stay|window,front-door", junk) && junk.bypassed.size() == 2);
  // sabotage: forcing an arming does not leave a cut loop out, and a zone that is left out still sounds when its wire is cut
  Panel cut(quick(), house(), "1234");
  cut.set_zone("window", ZoneState::kTamper, 0);   // 24 hours: it sounds even disarmed
  CHECK(cut.phase() == Phase::kAlarm);
  CHECK(cut.disarm("1234", 1) == Refusal::kNone);
  cut.set_zone("window", ZoneState::kTamper, 2);   // no change
  Panel forced(quick(), house(), "1234");
  forced.set_zone("hall-motion", ZoneState::kTriggered, 0);
  CHECK(forced.arm(Mode::kAway, true, 1) == Refusal::kNone && forced.bypassed("hall-motion"));
  forced.tick(11000);
  forced.set_zone("hall-motion", ZoneState::kTamper, 12000);
  CHECK(forced.phase() == Phase::kAlarm);
  Panel blocked(quick(), house(), "1234");
  blocked.set_zone("front-door", ZoneState::kTamper, 0);   // tamper raises an alarm even disarmed ...
  blocked.disarm("1234", 1);
  blocked.set_zone("front-door", ZoneState::kTamper, 2);
  Panel stuck(quick(), house(), "1234");
  stuck.set_zone("window", ZoneState::kNormal, 0);
  stuck.set_zone("hall-motion", ZoneState::kTriggered, 0);
  stuck.set_zone("window", ZoneState::kTamper, 1); stuck.disarm("1234", 2);   // the loop stays cut after the alarm was silenced
  CHECK(stuck.arm(Mode::kAway, true, 3) == Refusal::kZonesOpen && stuck.phase() == Phase::kDisarmed);
}

// ---- a long alarm, an alarm with the system disarmed and a lockout across a restart ------------------------------------------------------------

static void test_resound_and_restore_more() {
  // the siren timed out, then another zone sounds: it sounds again (and is noted), at most three times
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0); p.tick(11000);
  p.set_zone("window", ZoneState::kTriggered, 12000);
  CHECK(p.phase() == Phase::kAlarm && p.siren());
  p.tick(12000 + 61000);
  CHECK(p.phase() == Phase::kAlarm && !p.siren());
  p.set_zone("hall-motion", ZoneState::kTriggered, 80000);
  CHECK(p.siren());
  std::size_t alarms = 0;
  for (const Event& e : p.events()) if (e.kind == EventKind::kAlarm) ++alarms;
  CHECK(alarms == 2);
  // the same zone sounding again while the siren is on changes nothing
  p.set_zone("hall-motion", ZoneState::kNormal, 81000);
  p.set_zone("hall-motion", ZoneState::kTriggered, 82000);
  alarms = 0;
  for (const Event& e : p.events()) if (e.kind == EventKind::kAlarm) ++alarms;
  CHECK(alarms == 2);
  CHECK(p.disarm("1234", 90000) == Refusal::kNone && !p.siren());

  // an alarm of smoke with the system disarmed comes back as the same alarm after a restart
  Panel smoke(quick(), house(), "1234");
  smoke.set_zone("smoke", ZoneState::kTriggered, 0);
  CHECK(smoke.phase() == Phase::kAlarm && smoke.mode() == Mode::kDisarmed);
  Panel::Saved saved = smoke.saved();
  Panel::Saved read;
  CHECK(saved.phase == Phase::kAlarm && saved.mode == Mode::kDisarmed && parse_saved(saved_text(saved), read) && read == saved);
  Panel back(quick(), house(), "1234");
  CHECK(back.restore(read, 5) && back.phase() == Phase::kAlarm && back.mode() == Mode::kDisarmed && back.siren());

  // an alarm whose siren had timed out comes back without the siren
  smoke.tick(61000);
  saved = smoke.saved();
  CHECK(saved.silenced && parse_saved(saved_text(saved), read) && read.silenced);
  Panel quiet(quick(), house(), "1234");
  CHECK(quiet.restore(read, 5) && quiet.phase() == Phase::kAlarm && !quiet.siren());

  // a lockout comes back after a restart, and the right PIN does not get through it
  Panel locked(quick(), house(), "1234");
  for (int i = 0; i < 3; ++i) locked.disarm("0000", 1000 + i);
  CHECK(locked.locked_out(2000));
  saved = locked.saved();
  CHECK(saved.lockout == 1 && saved.phase == Phase::kDisarmed && parse_saved(saved_text(saved), read) && read == saved);
  Panel after(quick(), house(), "1234");
  after.restore(read, 10);
  CHECK(after.locked_out(11) && after.disarm("1234", 11) == Refusal::kLockedOut && !after.locked_out(10 + 31000));
  CHECK(!parse_saved("armed|away||nonsense", read) && !parse_saved("armed|away||silenced|lock1", read));
}

int main() {
  test_loop();
  test_zone_names();
  test_debounce();
  test_arm_and_disarm();
  test_instant_zone();
  test_entry_delay();
  test_exit_delay();
  test_modes();
  test_always_zones_and_tamper();
  test_open_zones_at_arming();
  test_pin_and_lockout();
  test_lockout_grows_until_a_right_pin();
  test_siren_times_out();
  test_silence_keeps_the_alarm();
  test_silence_counts_wrong_pins();
  test_zero_delays_and_log();
  test_restart_and_tamper();
  test_resound_and_restore_more();
  test_random();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
