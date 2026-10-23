// ARMOR-NODE-ALARM - host tests of the arming profiles: how a profile is read, what it changes in the panel and, as important, what it never changes.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Nothing here has run on a real installation: the zones are stand-ins and the clock is a number the test moves.
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "../core/alarm_config.hpp"
#include "../core/alarm_message.hpp"
#include "../core/alarm_panel.hpp"
#include "../core/alarm_runner.hpp"
#include "../core/arm_profile.hpp"

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
  return {{"front-door", "Front door", ZoneKind::kEntry}, {"window", "Living room window", ZoneKind::kInstant}, {"hall-motion", "Hall motion", ZoneKind::kInterior},
          {"smoke", "Kitchen smoke detector", ZoneKind::kAlways}, {"garage", "Garage door", ZoneKind::kInstant}};
}
static Timing quick() { Timing t; t.exit_delay_s = 10; t.entry_delay_s = 10; t.siren_max_s = 60; t.lockout_after = 3; t.lockout_s = 30; return t; }
static bool has_event(const Panel& p, EventKind kind) { for (const Event& e : p.events()) if (e.kind == kind) return true; return false; }

static bool read(const std::string& text, ArmProfile& out) { return parse_profile_text(text, out); }

// ---- reading a profile -----------------------------------------------------------------------------------------------------------------

static void test_parse() {
  ArmProfile p;
  CHECK(read("{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\"}", p) && p.id == "away" && p.name == "Away" && !p.stay && p.exit_delay_s == -1 && p.entry_delay_s == -1 && p.siren && p.notify_urgent && p.zones.empty());
  CHECK(read("{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\",\"exit_delay_s\":0,\"entry_delay_s\":300,\"siren\":false,\"notify\":\"normal\","
             "\"zones\":[{\"id\":\"hall-motion\",\"armed\":true},{\"id\":\"front-door\",\"delay\":\"instant\"},{\"id\":\"garage\",\"armed\":false}]}", p));
  CHECK(p.stay && p.exit_delay_s == 0 && p.entry_delay_s == 300 && !p.siren && !p.notify_urgent && p.zones.size() == 3);
  CHECK(p.zones[0].armed == ArmedOverride::kYes && p.zones[0].delay == DelayOverride::kKind && p.zones[1].delay == DelayOverride::kInstant && p.zones[2].armed == ArmedOverride::kNo);
  // a name counted in characters, not bytes: 32 of them is the limit
  CHECK(read("{\"id\":\"x\",\"name\":\"" + std::string(32, 'n') + "\",\"base\":\"away\"}", p) && !read("{\"id\":\"x\",\"name\":\"" + std::string(33, 'n') + "\",\"base\":\"away\"}", p));
  std::string accents = "{\"id\":\"x\",\"name\":\"";
  for (int i = 0; i < 32; ++i) accents += "\xC3\xA9";
  CHECK(read(accents + "\",\"base\":\"away\"}", p));
  // everything else is not a profile
  for (const std::string& bad : std::vector<std::string>{
           "{}", "[]", "\"away\"", "{\"id\":\"away\",\"name\":\"Away\"}", "{\"id\":\"away\",\"base\":\"away\"}", "{\"name\":\"Away\",\"base\":\"away\"}",
           "{\"id\":\"Away\",\"name\":\"Away\",\"base\":\"away\"}", "{\"id\":\"-away\",\"name\":\"Away\",\"base\":\"away\"}", "{\"id\":\"aaaaaaaaaaaaaaaaa\",\"name\":\"Away\",\"base\":\"away\"}",
           "{\"id\":\"away\",\"name\":\"\",\"base\":\"away\"}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"night\"}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"disarmed\"}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"pin\":\"1234\"}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"exit_delay_s\":301}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"exit_delay_s\":-1}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"entry_delay_s\":1.5}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"exit_delay_s\":\"10\"}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"siren\":\"off\"}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"notify\":\"silent\"}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":{}}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"armed\":true}]}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"id\":\"Bad Zone\"}]}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"id\":\"a\",\"armed\":1}]}", "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"id\":\"a\",\"delay\":\"later\"}]}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"id\":\"a\",\"name\":\"x\"}]}",
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"id\":\"a\",\"armed\":true},{\"id\":\"a\",\"armed\":false}]}",     // a zone named twice
           "{\"id\":\"away\",\"name\":\"Away\",\"base\":\"away\",\"zones\":[{\"id\":\"a\",\"armed\":false,\"delay\":\"entry\"}]}"})            // a zone left out has no delay
    CHECK(!read(bad, p));
  std::string many = "{\"id\":\"x\",\"name\":\"X\",\"base\":\"away\",\"zones\":[";
  for (int i = 0; i < 17; ++i) many += std::string(i ? "," : "") + "{\"id\":\"z" + std::to_string(i) + "\"}";
  CHECK(!read(many + "]}", p));
  many = "{\"id\":\"x\",\"name\":\"X\",\"base\":\"away\",\"zones\":[";
  for (int i = 0; i < 16; ++i) many += std::string(i ? "," : "") + "{\"id\":\"z" + std::to_string(i) + "\"}";
  CHECK(read(many + "]}", p) && p.zones.size() == 16);
  CHECK(!read(std::string(3000, ' ') + "{}", p));
  // what is written is what is read back
  ArmProfile night;
  CHECK(read("{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\",\"entry_delay_s\":45,\"siren\":false,\"zones\":[{\"id\":\"hall-motion\",\"armed\":true},{\"id\":\"garage\",\"armed\":false}]}", night));
  ArmProfile again;
  CHECK(read(profile_json(night), again) && profile_json(again) == profile_json(night) && again.stay && !again.siren && again.entry_delay_s == 45 && again.exit_delay_s == -1 && again.zones.size() == 2);
}

// ---- which zones guard ----------------------------------------------------------------------------------------------------------------

static void test_resolve() {
  // with no profile it is exactly what the kinds always meant
  for (const bool stay : {false, true}) {
    CHECK(resolve(ZoneKind::kInstant, stay, nullptr, "z").armed && !resolve(ZoneKind::kInstant, stay, nullptr, "z").delayed);
    CHECK(resolve(ZoneKind::kEntry, stay, nullptr, "z").armed && resolve(ZoneKind::kEntry, stay, nullptr, "z").delayed);
    CHECK(resolve(ZoneKind::kInterior, stay, nullptr, "z").armed == !stay && !resolve(ZoneKind::kInterior, stay, nullptr, "z").delayed);
    CHECK(resolve(ZoneKind::kAlways, stay, nullptr, "z").armed && !resolve(ZoneKind::kAlways, stay, nullptr, "z").delayed);
  }
  ArmProfile p;
  CHECK(read("{\"id\":\"x\",\"name\":\"X\",\"base\":\"stay\",\"zones\":[{\"id\":\"hall\",\"armed\":true},{\"id\":\"win\",\"armed\":false},{\"id\":\"door\",\"delay\":\"instant\"},{\"id\":\"back\",\"delay\":\"entry\"},"
             "{\"id\":\"smoke\",\"armed\":false},{\"id\":\"smoke2\",\"delay\":\"entry\"}]}", p));
  CHECK(resolve(ZoneKind::kInterior, true, &p, "hall").armed);                                    // an interior zone switched on in stay
  CHECK(!resolve(ZoneKind::kInterior, true, &p, "other").armed);                                  // the others keep the base
  CHECK(!resolve(ZoneKind::kInstant, true, &p, "win").armed && resolve(ZoneKind::kInstant, true, &p, "other").armed);
  CHECK(!resolve(ZoneKind::kEntry, true, &p, "door").delayed && resolve(ZoneKind::kEntry, true, &p, "other").delayed);
  CHECK(resolve(ZoneKind::kInstant, true, &p, "back").delayed && resolve(ZoneKind::kInstant, true, &p, "back").armed);
  CHECK(resolve(ZoneKind::kAlways, true, &p, "smoke").armed && !resolve(ZoneKind::kAlways, true, &p, "smoke2").delayed);   // smoke and panic are never switched off or delayed
}

// ---- the panel ------------------------------------------------------------------------------------------------------------------------

static ArmProfile night() {
  ArmProfile p;
  read("{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\",\"entry_delay_s\":20,\"zones\":[{\"id\":\"hall-motion\",\"armed\":true},{\"id\":\"garage\",\"armed\":false},{\"id\":\"window\",\"delay\":\"entry\"}]}", p);
  return p;
}

static void test_zones_in_a_profile() {
  const ArmProfile profile = night();
  Panel p(quick(), house(), "1234");
  CHECK(p.arm(Mode::kStay, false, 0, &profile) == Refusal::kNone && p.profile() != nullptr && p.profile()->id == "night");
  p.tick(11000);
  CHECK(p.phase() == Phase::kArmed);
  p.set_zone("garage", ZoneState::kTriggered, 12000);                          // left out in this profile: nothing happens
  CHECK(p.phase() == Phase::kArmed && !p.siren());
  p.set_zone("garage", ZoneState::kNormal, 12500);
  p.set_zone("window", ZoneState::kTriggered, 13000);                          // an instant zone made a delayed one: the entry delay starts instead of the alarm
  CHECK(p.phase() == Phase::kEntryDelay && !p.siren() && has_event(p, EventKind::kEntryDelayStarted));
  p.tick(32999); CHECK(p.phase() == Phase::kEntryDelay);                       // the profile's entry delay (20 s), not the node's (10 s)
  p.tick(33000); CHECK(p.phase() == Phase::kAlarm && p.siren());
  CHECK(p.disarm("1234", 34000) == Refusal::kNone && p.profile() == nullptr && p.phase() == Phase::kDisarmed);   // a disarm ends the profile

  // an interior zone switched on in "stay" alarms (and a plain stay would have ignored it)
  Panel q(quick(), house(), "1234");
  q.arm(Mode::kStay, false, 0, &profile); q.tick(11000);
  q.set_zone("hall-motion", ZoneState::kTriggered, 12000);
  CHECK(q.phase() == Phase::kAlarm && q.events().back().zone == "hall-motion");
  Panel plain(quick(), house(), "1234");
  plain.arm(Mode::kStay, false, 0); plain.tick(11000);
  plain.set_zone("hall-motion", ZoneState::kTriggered, 12000);
  CHECK(plain.phase() == Phase::kArmed && plain.profile() == nullptr);

  // the entry door made instant alarms at once
  ArmProfile instant;
  CHECK(read("{\"id\":\"i\",\"name\":\"I\",\"base\":\"away\",\"zones\":[{\"id\":\"front-door\",\"delay\":\"instant\"}]}", instant));
  Panel r(quick(), house(), "1234");
  r.arm(Mode::kAway, false, 0, &instant); r.tick(11000);
  r.set_zone("front-door", ZoneState::kTriggered, 12000);
  CHECK(r.phase() == Phase::kAlarm && !has_event(r, EventKind::kEntryDelayStarted));

  // the delays of the profile: no exit delay arms at once; an entry delay of 0 alarms at once
  ArmProfile quick_profile;
  CHECK(read("{\"id\":\"q\",\"name\":\"Q\",\"base\":\"away\",\"exit_delay_s\":0,\"entry_delay_s\":0}", quick_profile));
  Panel s(quick(), house(), "1234");
  CHECK(s.arm(Mode::kAway, false, 0, &quick_profile) == Refusal::kNone && s.phase() == Phase::kArmed);
  s.set_zone("front-door", ZoneState::kTriggered, 1000);
  CHECK(s.phase() == Phase::kAlarm);
  ArmProfile slow;
  CHECK(read("{\"id\":\"s\",\"name\":\"S\",\"base\":\"away\",\"exit_delay_s\":60}", slow));
  Panel t(quick(), house(), "1234");
  t.arm(Mode::kAway, false, 0, &slow); t.tick(59999); CHECK(t.phase() == Phase::kExitDelay); t.tick(60000); CHECK(t.phase() == Phase::kArmed);
}

static void test_arming_with_open_zones() {
  const ArmProfile profile = night();
  // a zone the profile leaves out does not stop the arming, a window does
  Panel p(quick(), house(), "1234");
  p.set_zone("garage", ZoneState::kTriggered, 0);
  CHECK(p.open_zones(Mode::kStay).size() == 1);                                           // a plain stay is stopped by the garage door...
  CHECK(p.open_zones(Mode::kStay, &profile).empty());                                     // ...this profile is not
  CHECK(p.arm(Mode::kStay, false, 1000) == Refusal::kZonesOpen);
  CHECK(p.arm(Mode::kStay, false, 1000, &profile) == Refusal::kNone);
  // a zone in trouble and a tamper still stop it, in the zone the profile leaves out as in any other
  Panel q(quick(), house(), "1234");
  q.set_zone("garage", ZoneState::kTrouble, 0);
  CHECK(q.arm(Mode::kStay, false, 1000, &profile) == Refusal::kZonesOpen);
  CHECK(q.arm(Mode::kStay, true, 1000, &profile) == Refusal::kNone && q.bypassed("garage"));      // forced: left out until the system is disarmed
  Panel r(quick(), house(), "1234");
  r.set_zone("garage", ZoneState::kTamper, 0);
  CHECK(r.phase() == Phase::kAlarm);                                                       // a tamper alarms even disarmed
  Panel u(quick(), house(), "1234");
  u.set_zone("window", ZoneState::kTriggered, 0);
  CHECK(u.arm(Mode::kStay, false, 1000, &profile) == Refusal::kZonesOpen);                  // a profile does not make an open window harmless
  // a sounding always zone is never left out, forced or not, with or without a profile
  Panel v(quick(), house(), "1234");
  v.set_zone("smoke", ZoneState::kTriggered, 0);
  CHECK(v.disarm("1234", 500) == Refusal::kNone && v.phase() == Phase::kDisarmed);   // the alarm is ended; the detector is still sounding
  CHECK(v.arm(Mode::kStay, true, 1000, &profile) == Refusal::kZonesOpen && v.arm(Mode::kAway, true, 1000) == Refusal::kZonesOpen);
  // a profile whose base is not the mode is not applied
  Panel w(quick(), house(), "1234");
  CHECK(w.arm(Mode::kAway, false, 0, &profile) == Refusal::kNone && w.profile() == nullptr && w.mode() == Mode::kAway);
}

static void test_the_siren_of_a_profile() {
  ArmProfile silent;
  CHECK(read("{\"id\":\"quiet\",\"name\":\"Quiet\",\"base\":\"away\",\"siren\":false}", silent));
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kAway, false, 0, &silent); p.tick(11000);
  p.set_zone("window", ZoneState::kTriggered, 12000);
  CHECK(p.phase() == Phase::kAlarm && !p.siren() && has_event(p, EventKind::kAlarm));    // a silent alarm: latched and recorded
  p.set_zone("window", ZoneState::kNormal, 12500);
  p.set_zone("window", ZoneState::kTriggered, 13000);                                    // the same zone again: nothing new
  const std::size_t events = p.events().size();
  p.set_zone("garage", ZoneState::kTriggered, 14000);                                    // another zone: news, still silent
  CHECK(!p.siren() && p.events().size() == events + 1);
  p.set_zone("smoke", ZoneState::kTriggered, 15000);                                     // smoke always sounds
  CHECK(p.siren());
  p.set_zone("hall-motion", ZoneState::kTriggered, 16000);
  CHECK(p.siren());                                                                       // a silent zone never switches a sounding siren off
  CHECK(p.silence("1234", 17000) == Refusal::kNone && !p.siren() && p.phase() == Phase::kAlarm);   // the silence is the PIN's, as always
  CHECK(p.disarm("0000", 18000) == Refusal::kBadPin && p.disarm("1234", 19000) == Refusal::kNone && p.profile() == nullptr);

  // a tamper sounds, a silent profile or not
  Panel t(quick(), house(), "1234");
  t.arm(Mode::kAway, false, 0, &silent); t.tick(11000);
  t.set_zone("window", ZoneState::kTamper, 12000);
  CHECK(t.siren());
  // the silent alarm after the entry delay is silent too
  ArmProfile silent_door;
  CHECK(read("{\"id\":\"quiet\",\"name\":\"Quiet\",\"base\":\"away\",\"siren\":false}", silent_door));
  Panel d(quick(), house(), "1234");
  d.arm(Mode::kAway, false, 0, &silent_door); d.tick(11000);
  d.set_zone("front-door", ZoneState::kTriggered, 12000); d.tick(22000);
  CHECK(d.phase() == Phase::kAlarm && !d.siren());
  // a silence of a silent alarm changes nothing and needs the PIN like any other
  CHECK(d.silence("0000", 23000) == Refusal::kBadPin && d.silence("1234", 24000) == Refusal::kNone && !d.siren());
  // and the PIN rules are the same under a profile: three wrong ones lock it out
  Panel l(quick(), house(), "1234");
  l.arm(Mode::kStay, false, 0, &silent_door); l.tick(11000);
  for (int i = 0; i < 3; ++i) l.disarm("0000", 20000 + i);
  CHECK(l.locked_out(20010) && l.disarm("1234", 20010) == Refusal::kLockedOut);
}

static void test_a_restart_keeps_the_profile() {
  const ArmProfile profile = night();
  Panel p(quick(), house(), "1234");
  p.arm(Mode::kStay, false, 0, &profile); p.tick(11000);
  const Panel::Saved saved = p.saved();
  Panel after(quick(), house(), "1234");
  CHECK(after.restore(saved, 5, &profile) && after.profile() != nullptr && after.profile()->id == "night" && after.mode() == Mode::kStay);
  after.set_zone("hall-motion", ZoneState::kTriggered, 100);
  CHECK(after.phase() == Phase::kAlarm);                                              // the restored house guards as the profile said
  Panel wrong(quick(), house(), "1234");
  CHECK(wrong.restore(saved, 5, nullptr) && wrong.profile() == nullptr);              // no profile kept: a plain stay
  ArmProfile away = profile;
  away.stay = false;
  Panel mismatch(quick(), house(), "1234");
  CHECK(mismatch.restore(saved, 5, &away) && mismatch.profile() == nullptr);          // a profile of another mode is never applied
  // a silent alarm restarts silent; a profile does not come back on a disarmed panel
  ArmProfile silent;
  CHECK(read("{\"id\":\"quiet\",\"name\":\"Quiet\",\"base\":\"away\",\"siren\":false}", silent));
  Panel s(quick(), house(), "1234");
  s.arm(Mode::kAway, false, 0, &silent); s.tick(11000); s.set_zone("window", ZoneState::kTriggered, 12000);
  const Panel::Saved ringing = s.saved();
  CHECK(ringing.phase == Phase::kAlarm && ringing.silenced);
  Panel back(quick(), house(), "1234");
  CHECK(back.restore(ringing, 5, &silent) && back.phase() == Phase::kAlarm && !back.siren());
  Panel disarmed(quick(), house(), "1234");
  CHECK(!disarmed.restore(Panel::Saved{}, 5, &profile) && disarmed.profile() == nullptr);
}

// ---- it changes nothing for an arm without a profile ----------------------------------------------------------------------------------

// The same random run on two panels, one armed plainly and one with a profile that says nothing but its base: they must do exactly the same, step by step.
static void test_a_neutral_profile_changes_nothing() {
  for (const bool stay : {false, true}) {
    ArmProfile neutral;
    CHECK(read(std::string("{\"id\":\"n\",\"name\":\"N\",\"base\":\"") + (stay ? "stay" : "away") + "\"}", neutral));
    for (unsigned seed = 1; seed <= 40; ++seed) {
      std::mt19937 random(seed * 7919u + (stay ? 1u : 0u));
      Panel a(quick(), house(), "1234"), b(quick(), house(), "1234");
      std::uint64_t now = 0;
      const std::vector<Zone> zones = house();
      for (int step = 0; step < 400; ++step) {
        now += 100 + random() % 2500;
        const unsigned dice = random() % 20;
        if (dice < 9) {
          const std::string id = zones[random() % zones.size()].id;
          const ZoneState state = static_cast<ZoneState>(random() % 4);
          a.set_zone(id, state, now); b.set_zone(id, state, now);
        } else if (dice == 9) {
          const bool force = random() % 2 == 0;
          const Refusal ra = a.arm(stay ? Mode::kStay : Mode::kAway, force, now), rb = b.arm(stay ? Mode::kStay : Mode::kAway, force, now, &neutral);
          CHECK(ra == rb);
        } else if (dice == 10) {
          const std::string pin = random() % 3 ? "1234" : "0000";
          CHECK(a.disarm(pin, now) == b.disarm(pin, now));
        } else if (dice == 11) {
          const std::string pin = random() % 3 ? "1234" : "0000";
          CHECK(a.silence(pin, now) == b.silence(pin, now));
        } else {
          a.tick(now); b.tick(now);
        }
        const bool same = a.phase() == b.phase() && a.mode() == b.mode() && a.siren() == b.siren() && a.events().size() == b.events().size() && a.open_zones(Mode::kAway) == b.open_zones(Mode::kAway) && a.open_zones(Mode::kStay) == b.open_zones(Mode::kStay);
        if (!same) { CHECK(same); break; }
        for (std::size_t i = 0; i < a.events().size(); ++i) if (a.events()[i].kind != b.events()[i].kind || a.events()[i].zone != b.events()[i].zone) { CHECK(false); break; }
      }
    }
  }
}

// ---- the node: the command, the state, the restart ------------------------------------------------------------------------------------

static const double kClosed = 800, kOpen = 1800;

static config::Settings node_house() {
  config::Settings s = config::default_settings("a1b2c3");
  const char* names[] = {"front-door", "window", "hall-motion", "smoke"};
  const ZoneKind kinds[] = {ZoneKind::kEntry, ZoneKind::kInstant, ZoneKind::kInterior, ZoneKind::kAlways};
  for (std::size_t i = 0; i < 4; ++i) { s.zones[i].enabled = true; s.zones[i].id = names[i]; s.zones[i].name = std::string("Zone ") + names[i]; s.zones[i].kind = kinds[i]; }
  s.relays[0] = {true, "siren", "Siren", board::kRelayPins[0], false};
  s.panel.siren_relay = "siren";
  s.panel.pin = "1234";
  s.panel.timing.exit_delay_s = 10; s.panel.timing.entry_delay_s = 10; s.panel.timing.siren_max_s = 60;
  s.server.commands = true;
  return s;
}

static std::string command(const std::string& body) { return "{\"kind\":\"alarm_command\",\"node_id\":\"alarm-a1b2c3\",\"timestamp_ms\":7000,\"command_id\":\"c0ffee0123456789\"," + body + "}"; }

static void test_the_command() {
  const std::string night_json = "{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\",\"entry_delay_s\":20,\"zones\":[{\"id\":\"hall-motion\",\"armed\":true},{\"id\":\"window\",\"armed\":false}]}";
  Command c;
  CHECK(parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":" + night_json), "alarm-a1b2c3", c) && c.has_profile && c.profile.id == "night" && c.mode == Mode::kStay && c.profile.stay);
  CHECK(parse_command(command("\"action\":\"arm\",\"mode\":\"stay\""), "alarm-a1b2c3", c) && !c.has_profile);               // without one it is the command it always was
  CHECK(parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"force\":true,\"profile\":" + night_json), "alarm-a1b2c3", c) && c.force && c.has_profile);
  // a profile whose base is not the mode, a profile on anything but an arm, and a profile that is not exactly the contract's are not commands
  for (const std::string& bad : std::vector<std::string>{
           command("\"action\":\"arm\",\"mode\":\"away\",\"profile\":" + night_json), command("\"action\":\"disarm\",\"profile\":" + night_json), command("\"action\":\"silence\",\"profile\":" + night_json),
           command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":\"night\""), command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":{}"), command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":null"),
           command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\",\"pin\":\"1234\"}")})
    CHECK(!parse_command(bad, "alarm-a1b2c3", c));
}

static void run(Runner& node, std::uint64_t& now, std::uint64_t ms, const std::vector<double>& readings) {
  const std::uint64_t until = now + ms;
  while (now < until) {
    for (std::size_t i = 0; i < readings.size(); ++i) node.feed(i, readings[i], now);
    node.tick(now);
    now += 100;
  }
}

static void test_the_node_arms_with_a_profile() {
  const std::string night_json = "{\"id\":\"night\",\"name\":\"Night\",\"base\":\"stay\",\"entry_delay_s\":20,\"zones\":[{\"id\":\"hall-motion\",\"armed\":true},{\"id\":\"window\",\"armed\":false}]}";
  Runner node(node_house());
  std::uint64_t now = 0;
  const std::vector<double> quiet{kClosed, kClosed, kClosed, kClosed};
  run(node, now, 1000, quiet);
  // the state of a disarmed panel says it understands profiles and has none
  json::Value state;
  CHECK(json::parse(node.state_json(1000, now), state) && state.bool_or("arm_profiles", false) && state.get("profile") == nullptr);
  // the window is open: a plain stay is refused, this profile (which leaves the window out) is not
  run(node, now, 500, {kClosed, kOpen, kClosed, kClosed});
  CHECK(node.handle(command("\"action\":\"arm\",\"mode\":\"stay\""), now).refusal == Refusal::kZonesOpen);
  const Reply reply = node.handle(command("\"action\":\"arm\",\"mode\":\"stay\",\"profile\":" + night_json), now);
  CHECK(reply.understood && reply.refusal == Refusal::kNone && node.panel().profile() != nullptr && node.panel().mode() == Mode::kStay);
  CHECK(json::parse(node.state_json(2000, now), state) && state.string_or("profile", "") == "night" && state.string_or("mode", "") == "stay" && state.get("open_zones")->items.empty());
  json::Value answer;
  CHECK(json::parse(node.result_json(1, reply), answer) && answer.bool_or("accepted", false) && answer.string_or("phase", "") == "exit_delay");
  run(node, now, 11000, {kClosed, kOpen, kClosed, kClosed});
  CHECK(node.panel().phase() == Phase::kArmed);
  // the hall's motion alarms in this stay, and the node restarts into the same profile
  const Panel::Saved saved = node.saved();
  const ArmProfile kept = *node.profile();
  Runner after(node_house());
  CHECK(after.restore(saved, 0, &kept) && after.profile() != nullptr && after.profile()->id == "night");
  run(after, now, 500, {kClosed, kClosed, kOpen, kClosed});
  CHECK(after.panel().phase() == Phase::kAlarm);
  // the PIN and the server's disarm are what they were; the profile goes with the disarm
  CHECK(after.disarm("0000", now) == Refusal::kBadPin && after.panel().profile() != nullptr);
  const Reply off = after.handle(command("\"action\":\"disarm\""), now);
  CHECK(off.understood && off.refusal == Refusal::kNone && after.panel().profile() == nullptr);
  CHECK(json::parse(after.state_json(3000, now), state) && state.get("profile") == nullptr);
  // the panel's own page arms without a profile
  Runner own(node_house());
  CHECK(own.arm(Mode::kAway, false, 0) == Refusal::kNone && own.panel().profile() == nullptr);
  // a command that says a mode and a profile of another base gets no answer
  const Reply wrong = own.handle(command("\"action\":\"arm\",\"mode\":\"away\",\"profile\":" + night_json), 5);
  CHECK(!wrong.understood);
}

int main() {
  test_parse();
  test_resolve();
  test_zones_in_a_profile();
  test_arming_with_open_zones();
  test_the_siren_of_a_profile();
  test_a_restart_keeps_the_profile();
  test_a_neutral_profile_changes_nothing();
  test_the_command();
  test_the_node_arms_with_a_profile();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
