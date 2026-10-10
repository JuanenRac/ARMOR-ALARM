// ARMOR-ALARM - host tests of the node: its settings (what the page edits and the flash keeps), its messages and the runner that ties the wired zones to the panel.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// No ADC, relay or broker was used: the readings and the clock are numbers the test moves.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/alarm_config.hpp"
#include "../core/alarm_message.hpp"
#include "../core/alarm_runner.hpp"

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

static config::Settings valid_settings() {
  config::Settings s = config::default_settings("a1b2c3");
  s.sta.enabled = true; s.sta.ssid = "casa"; s.sta.password = "una-clave-larga";
  return s;
}
static bool has(const config::Problems& problems, const std::string& path, const std::string& code) {
  for (const config::Problem& p : problems) if (p.path == path && p.code == code) return true;
  return false;
}
static bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// A node with a house: a door you come in through, a window, a hall's motion and a smoke detector, a siren on the first relay, a PIN.
static config::Settings house() {
  config::Settings s = valid_settings();
  const char* names[] = {"front-door", "window", "hall-motion", "smoke"};
  const alarm::ZoneKind kinds[] = {alarm::ZoneKind::kEntry, alarm::ZoneKind::kInstant, alarm::ZoneKind::kInterior, alarm::ZoneKind::kAlways};
  for (std::size_t i = 0; i < 4; ++i) { s.zones[i].enabled = true; s.zones[i].id = names[i]; s.zones[i].name = std::string("Zone ") + names[i]; s.zones[i].kind = kinds[i]; }
  s.relays[0] = {true, "siren", "Siren", board::kRelayPins[0], false};
  s.panel.siren_relay = "siren";
  s.panel.pin = "1234";
  s.panel.timing.exit_delay_s = 10; s.panel.timing.entry_delay_s = 10; s.panel.timing.siren_max_s = 60;
  return s;
}

// ---- the settings --------------------------------------------------------------------------------------------------------------------

static void test_defaults() {
  const config::Settings s = config::default_settings("a1b2c3");
  CHECK(s.node_id == "alarm-a1b2c3" && s.node_name == s.node_id && s.zones.size() == 8 && std::string(s.ap.ssid) == "ARMOR-ALARM");
  CHECK(s.zones[0].id == "zone1" && s.zones[7].id == "zone8" && !s.zones[0].enabled && s.zones[0].pin == board::kAnalogPins[0] && s.zones[0].hold_ms == 200);
  CHECK(!s.zigbee.enabled && !s.remote.relays && !s.server.commands && s.panel.pin.empty() && s.panel.siren_relay.empty());
  CHECK(s.panel.timing.exit_delay_s == 30 && s.panel.timing.entry_delay_s == 30 && s.panel.timing.siren_max_s == 180 && alarm::limits_are_valid(s.loop));
  // nothing is guarded by default: the only thing a fresh node may lack is a way in on the Wi-Fi board
  for (const config::Problem& problem : config::validate(s)) CHECK(problem.path == "sta.enabled");
  CHECK(config::validate(valid_settings()).empty());
}

static void test_zones() {
  config::Settings s = house();
  CHECK(config::validate(s).empty());
  s.zones[1].id = "front-door"; CHECK(has(config::validate(s), "zones.1.id", "conflict")); s.zones[1].id = "window";
  s.zones[1].id = "Bad Id"; CHECK(has(config::validate(s), "zones.1.id", "invalid")); s.zones[1].id = "";
  CHECK(has(config::validate(s), "zones.1.id", "required")); s.zones[1].id = "window";
  s.zones[1].pin = s.zones[0].pin; CHECK(has(config::validate(s), "zones.1.pin", "conflict")); s.zones[1].pin = board::kAnalogPins[1];
  s.zones[1].pin = 16; CHECK(has(config::validate(s), "zones.1.pin", "not_analog")); s.zones[1].pin = board::kAnalogPins[1];   // ADC2 is never offered
  s.zones[1].pin = 33; CHECK(has(config::validate(s), "zones.1.pin", "reserved")); s.zones[1].pin = board::kAnalogPins[1];
  s.zones[1].pin = -1; CHECK(has(config::validate(s), "zones.1.pin", "required")); s.zones[1].pin = board::kAnalogPins[1];
  s.zones[1].name = std::string(200, 'x'); CHECK(has(config::validate(s), "zones.1.name", "invalid")); s.zones[1].name = "";
  CHECK(config::validate(s).empty());
  // a zone that is not enabled is not checked
  s.zones[5].id = "Bad Id"; s.zones[5].pin = 33; CHECK(config::validate(s).empty());
  // a zone's pin may not be one the base board uses for something else
  s.zones[5].pin = -1;
  s.zones[2].pin = board::kRelayPins[0]; CHECK(has(config::validate(s), "zones.2.pin", "not_analog") || has(config::validate(s), "zones.2.pin", "conflict")); s.zones[2].pin = board::kAnalogPins[2];
  // the loop's limits must be ascending and inside the converter's range
  s.loop.normal_below_mv = 300; CHECK(has(config::validate(s), "loop.short_below_mv", "range")); s.loop.normal_below_mv = 1200;
  CHECK(config::validate(s).empty());
}

static void test_panel_settings() {
  config::Settings s = house();
  // a PIN of four to twelve digits, required as soon as there is a zone to guard
  for (const char* bad : {"123", "1234567890123", "12a4", "12 34", ""}) { s.panel.pin = bad; CHECK(has(config::validate(s), "panel.pin", std::string(bad).empty() ? "required" : "invalid")); }
  for (const char* good : {"1234", "000000", "123456789012"}) { s.panel.pin = good; CHECK(config::validate(s).empty()); }
  s.panel.pin = "1234";
  config::Settings nothing = valid_settings();
  CHECK(config::validate(nothing).empty());   // no zone, no PIN needed
  // the siren is one of the enabled relays
  s.panel.siren_relay = "other"; CHECK(has(config::validate(s), "panel.siren_relay", "unknown_relay"));
  s.relays[1] = {false, "other", "", board::kRelayPins[1], false}; CHECK(has(config::validate(s), "panel.siren_relay", "unknown_relay"));   // a disabled one is not a siren
  s.panel.siren_relay = "siren"; CHECK(config::validate(s).empty());
  // the server's commands need a broker
  s.server.commands = true; CHECK(has(config::validate(s), "server.commands", "needs_broker"));
  s.mqtt.enabled = true; s.mqtt.uri = "mqtt://192.168.0.180:1883"; CHECK(config::validate(s).empty());
}

static void test_document() {
  config::Settings s = house();
  s.mqtt.enabled = true; s.mqtt.uri = "mqtt://192.168.0.180:1883"; s.server.commands = true;
  s.zones[3].hold_ms = 500; s.loop.triggered_below_mv = 2500; s.panel.timing.lockout_after = 5;
  s.zigbee.enabled = true; s.zigbee.allowed_client = "192.168.0.180"; s.zigbee.fallback_client = "192.168.0.60";
  CHECK(config::validate(s).empty());
  const std::string stored = config::to_json(s, true);
  config::Settings back; config::Problems problems;
  CHECK(config::load(stored, config::default_settings("a1b2c3"), back, problems) && problems.empty());
  CHECK(back.node_id == "alarm-a1b2c3" && back.zones[0].enabled && back.zones[0].kind == alarm::ZoneKind::kEntry && back.zones[3].kind == alarm::ZoneKind::kAlways && back.zones[3].hold_ms == 500 && !back.zones[4].enabled);
  CHECK(back.panel.pin == "1234" && back.panel.siren_relay == "siren" && back.panel.timing.lockout_after == 5 && back.server.commands && back.zigbee.fallback_client == "192.168.0.60" && back.loop.triggered_below_mv == 2500);
  CHECK(config::to_json(back, true) == stored);
  // the panel's copy never carries the PIN or a password, only that they exist
  const std::string shown = config::to_json(s, false);
  CHECK(!contains(shown, "1234") && !contains(shown, "una-clave-larga") && contains(shown, "\"pin_set\":true") && contains(shown, "\"password_set\":true"));
  // a document without a PIN keeps the stored one, "pin_clear" erases it, a new one replaces it
  config::Settings edited;
  problems.clear();
  CHECK(config::load("{\"panel\":{\"exit_delay_s\":20}}", s, edited, problems) && edited.panel.pin == "1234" && edited.panel.timing.exit_delay_s == 20);
  problems.clear();
  CHECK(config::load("{\"panel\":{\"pin\":\"987654\"}}", s, edited, problems) && edited.panel.pin == "987654");
  problems.clear();
  CHECK(!config::load("{\"panel\":{\"pin_clear\":true}}", s, edited, problems) && has(problems, "panel.pin", "required"));   // with zones to guard it may not be left empty
  // wrong types and ranges are refused by name
  for (const auto& test : std::vector<std::pair<std::string, std::string>>{
           {"{\"panel\":{\"exit_delay_s\":601}}", "panel.exit_delay_s"}, {"{\"panel\":{\"siren_max_s\":0}}", "panel.siren_max_s"}, {"{\"panel\":{\"lockout_after\":0}}", "panel.lockout_after"},
           {"{\"zones\":[{\"hold_ms\":10}]}", "zones.0.hold_ms"}, {"{\"zones\":[{\"hold_ms\":9000}]}", "zones.0.hold_ms"}, {"{\"loop\":{\"short_below_mv\":4000}}", "loop.short_below_mv"},
           {"{\"zones\":[{\"kind\":\"24h\"}]}", "zones.0.kind"}, {"{\"zones\":\"all\"}", "zones"}}) {
    problems.clear();
    CHECK(!config::load(test.first, s, edited, problems) && !problems.empty() && problems[0].path == test.second);
  }
  std::string nine = "{\"zones\":[";
  for (int i = 0; i < 9; ++i) nine += std::string(i ? "," : "") + "{}";
  problems.clear();
  CHECK(!config::load(nine + "]}", s, edited, problems) && has(problems, "zones", "invalid"));
  // an older or empty document loads with everything off
  problems.clear();
  CHECK(config::load("{\"v\":1,\"sta\":{\"enabled\":true,\"ssid\":\"casa\",\"password\":\"una-clave-larga\"}}", config::default_settings("a1b2c3"), edited, problems) && !edited.zones[0].enabled && !edited.server.commands);
}

// ---- the messages --------------------------------------------------------------------------------------------------------------------

static void test_state_and_commands() {
  config::Settings s = house();
  s.mqtt.enabled = true; s.mqtt.uri = "mqtt://192.168.0.180:1883"; s.server.commands = true;
  alarm::Runner node(s);
  const std::string state = node.state_json(1000, 5000);
  json::Value parsed;
  CHECK(json::parse(state, parsed));
  CHECK(parsed.string_or("kind", "") == "alarm" && parsed.string_or("node_id", "") == "alarm-a1b2c3" && parsed.string_or("phase", "") == "disarmed" && parsed.string_or("mode", "") == "disarmed");
  CHECK(!parsed.bool_or("siren", true) && !parsed.bool_or("locked_out", true) && parsed.bool_or("commands_enabled", false));
  const json::Value* zones = parsed.get("zones");
  CHECK(zones != nullptr && zones->items.size() == 4 && zones->items[0].string_or("id", "") == "front-door" && zones->items[0].string_or("kind", "") == "entry" && zones->items[0].string_or("state", "") == "normal");
  CHECK(parsed.get("open_zones") != nullptr && parsed.get("open_zones")->items.empty() && parsed.get("events") != nullptr && parsed.get("events")->items.empty());
  CHECK(alarm::topic("alarm-1", "state") == "armor/alarm/alarm-1/state" && alarm::topic("alarm-1", "command") == "armor/alarm/alarm-1/command" && alarm::topic("Bad Node", "state").empty());

  const std::string id = "c0ffee0123456789";
  const auto command = [&](const std::string& body) { return "{\"kind\":\"alarm_command\",\"node_id\":\"alarm-a1b2c3\",\"timestamp_ms\":7000,\"command_id\":\"" + id + "\"," + body + "}"; };
  alarm::Command c;
  CHECK(alarm::parse_command(command("\"action\":\"arm\",\"mode\":\"away\""), "alarm-a1b2c3", c) && c.action == alarm::Action::kArm && c.mode == alarm::Mode::kAway && !c.force && c.command_id == id);
  CHECK(alarm::parse_command(command("\"action\":\"arm\",\"mode\":\"stay\",\"force\":true"), "alarm-a1b2c3", c) && c.mode == alarm::Mode::kStay && c.force);
  CHECK(alarm::parse_command(command("\"action\":\"disarm\""), "alarm-a1b2c3", c) && c.action == alarm::Action::kDisarm);
  // anything that is not the contract's is not a command at all
  for (const std::string& bad : std::vector<std::string>{command("\"action\":\"arm\""), command("\"action\":\"arm\",\"mode\":\"night\""), command("\"action\":\"disarm\",\"mode\":\"away\""), command("\"action\":\"disarm\",\"force\":true"),
                                  command("\"action\":\"open\""), command("\"action\":\"arm\",\"mode\":\"away\",\"pin\":\"1234\""), command("\"action\":\"arm\",\"mode\":\"away\",\"force\":1"), command(""),
                                  std::string("{\"kind\":\"alarm_command\",\"node_id\":\"other\",\"timestamp_ms\":7000,\"command_id\":\"") + id + "\",\"action\":\"disarm\"}",
                                  std::string("{\"kind\":\"alarm_command\",\"node_id\":\"alarm-a1b2c3\",\"timestamp_ms\":7000,\"command_id\":\"short\",\"action\":\"disarm\"}"),
                                  std::string("{\"kind\":\"alarm_command\",\"node_id\":\"alarm-a1b2c3\",\"command_id\":\"") + id + "\",\"action\":\"disarm\"}", "not json", "[]", "{}"})
    CHECK(!alarm::parse_command(bad, "alarm-a1b2c3", c));
  // the answer
  const alarm::Reply reply = node.handle(command("\"action\":\"arm\",\"mode\":\"away\""), 6000);
  CHECK(reply.understood && reply.refusal == alarm::Refusal::kNone);
  json::Value answer;
  CHECK(json::parse(node.result_json(6001, reply), answer));
  CHECK(answer.string_or("kind", "") == "alarm_result" && answer.string_or("command_id", "") == id && answer.string_or("action", "") == "arm" && answer.bool_or("accepted", false) && answer.string_or("refusal", "") == "none" && answer.string_or("phase", "") == "exit_delay");
}

// ---- the runner ----------------------------------------------------------------------------------------------------------------------

static const double kClosed = 800, kOpen = 1800, kCut = 3000, kShort = 100;   // millivolts of a loop, with the default limits

// Feeds every zone with `reading` (or the given ones) for `ms`, one step of 100 ms at a time, as the firmware does.
static void run(alarm::Runner& node, std::uint64_t& now, std::uint64_t ms, const std::vector<double>& readings) {
  const std::uint64_t until = now + ms;
  while (now < until) {
    for (std::size_t i = 0; i < readings.size(); ++i) node.feed(i, readings[i], now);
    node.tick(now);
    now += 100;
  }
}

static void test_runner() {
  config::Settings s = house();
  alarm::Runner node(s);
  std::uint64_t now = 0;
  const std::vector<double> quiet{kClosed, kClosed, kClosed, kClosed};
  run(node, now, 1000, quiet);
  CHECK(node.panel().phase() == alarm::Phase::kDisarmed && !node.siren() && node.zones()[0].has_reading && node.zones()[0].state == alarm::ZoneState::kNormal && !node.zones()[4].has_reading);
  // arm, leave, and the window opens: the siren sounds
  CHECK(node.arm(alarm::Mode::kAway, false, now) == alarm::Refusal::kNone);
  run(node, now, 11000, quiet);
  CHECK(node.panel().phase() == alarm::Phase::kArmed && !node.siren());
  run(node, now, 300, {kClosed, kOpen, kClosed, kClosed});
  CHECK(node.panel().phase() == alarm::Phase::kAlarm && node.siren() && node.zones()[1].state == alarm::ZoneState::kTriggered);
  CHECK(node.disarm("0000", now) == alarm::Refusal::kBadPin && node.siren());
  CHECK(node.disarm("1234", now) == alarm::Refusal::kNone && !node.siren() && node.panel().phase() == alarm::Phase::kDisarmed);
}

static void test_debounce_and_tamper() {
  config::Settings s = house();
  alarm::Runner node(s);
  std::uint64_t now = 0;
  const std::vector<double> quiet{kClosed, kClosed, kClosed, kClosed};
  run(node, now, 1000, quiet);
  node.arm(alarm::Mode::kAway, false, now);
  run(node, now, 11000, quiet);
  // a bounce of 100 ms on the window (the zone holds 200 ms) is nothing
  run(node, now, 100, {kClosed, kOpen, kClosed, kClosed});
  run(node, now, 1000, quiet);
  CHECK(node.panel().phase() == alarm::Phase::kArmed && node.zones()[1].state == alarm::ZoneState::kNormal);
  // a cut wire on the hall motion is a tamper, which alarms in every state
  run(node, now, 400, {kClosed, kClosed, kCut, kClosed});
  CHECK(node.panel().phase() == alarm::Phase::kAlarm && node.zones()[2].state == alarm::ZoneState::kTamper && node.siren());
  node.disarm("1234", now);
  run(node, now, 1000, quiet);
  // a short does too, even disarmed
  run(node, now, 400, {kClosed, kClosed, kClosed, kShort});
  CHECK(node.panel().phase() == alarm::Phase::kAlarm && node.zones()[3].state == alarm::ZoneState::kTamper);
}

static void test_server_commands() {
  config::Settings s = house();
  s.mqtt.enabled = true; s.mqtt.uri = "mqtt://192.168.0.180:1883";
  const auto command = [&](const std::string& body) { return "{\"kind\":\"alarm_command\",\"node_id\":\"alarm-a1b2c3\",\"timestamp_ms\":7000,\"command_id\":\"c0ffee0123456789\"," + body + "}"; };
  // commands off (the default): nothing is understood, whatever it says
  alarm::Runner deaf(s);
  CHECK(!deaf.handle(command("\"action\":\"arm\",\"mode\":\"away\""), 0).understood && deaf.panel().phase() == alarm::Phase::kDisarmed);
  s.server.commands = true;
  alarm::Runner node(s);
  std::uint64_t now = 0;
  const std::vector<double> quiet{kClosed, kClosed, kClosed, kClosed};
  run(node, now, 1000, quiet);
  alarm::Reply arm = node.handle(command("\"action\":\"arm\",\"mode\":\"stay\""), now);
  CHECK(arm.understood && arm.refusal == alarm::Refusal::kNone && node.panel().mode() == alarm::Mode::kStay && node.panel().phase() == alarm::Phase::kExitDelay);
  CHECK(node.handle(command("\"action\":\"arm\",\"mode\":\"away\""), now).refusal == alarm::Refusal::kNotDisarmed);          // already arming
  run(node, now, 11000, quiet);
  // a command to disarm needs no PIN (the server's authority) and is not held back by a lockout
  for (int i = 0; i < 3; ++i) node.disarm("0000", now);
  CHECK(node.locked_out(now) && node.disarm("1234", now) == alarm::Refusal::kLockedOut);
  const alarm::Reply disarm = node.handle(command("\"action\":\"disarm\""), now);
  CHECK(disarm.understood && disarm.refusal == alarm::Refusal::kNone && node.panel().phase() == alarm::Phase::kDisarmed);
  // arming over an open window is refused, and forced it is accepted
  run(node, now, 500, {kClosed, kOpen, kClosed, kClosed});
  alarm::Reply refused = node.handle(command("\"action\":\"arm\",\"mode\":\"away\""), now);
  CHECK(refused.understood && refused.refusal == alarm::Refusal::kZonesOpen);
  json::Value answer;
  CHECK(json::parse(node.result_json(1, refused), answer) && !answer.bool_or("accepted", true) && answer.string_or("refusal", "") == "zones_open" && answer.string_or("phase", "") == "disarmed");
  CHECK(node.handle(command("\"action\":\"arm\",\"mode\":\"away\",\"force\":true"), now).refusal == alarm::Refusal::kNone);
  // a message that is not the contract's is not understood
  CHECK(!node.handle("{\"kind\":\"alarm_command\"}", now).understood && !node.handle("garbage", now).understood);
}

static void test_state_follows_the_panel() {
  config::Settings s = house();
  alarm::Runner node(s);
  std::uint64_t now = 0;
  const std::vector<double> quiet{kClosed, kClosed, kClosed, kClosed};
  run(node, now, 1000, quiet);
  node.arm(alarm::Mode::kAway, false, now);
  run(node, now, 11000, quiet);
  run(node, now, 300, {kClosed, kOpen, kClosed, kClosed});
  json::Value state;
  CHECK(json::parse(node.state_json(5, now), state));
  CHECK(state.string_or("phase", "") == "alarm" && state.string_or("mode", "") == "away" && state.bool_or("siren", false));
  const json::Value* zones = state.get("zones");
  CHECK(zones != nullptr && zones->items[1].string_or("state", "") == "triggered" && zones->items[0].string_or("state", "") == "normal");
  const json::Value* events = state.get("events");
  CHECK(events != nullptr && events->items.size() >= 3 && events->items.back().string_or("kind", "") == "alarm" && events->items.back().string_or("zone", "") == "window" && events->items.back().get("ago_s") != nullptr);
  // the events are the last ten, however many happened
  for (int i = 0; i < 30; ++i) node.disarm("0", now + static_cast<std::uint64_t>(i) * 1000);
  CHECK(json::parse(node.state_json(6, now + 100000), state) && state.get("events")->items.size() <= alarm::kMaxReportedEvents);
  // every enabled zone is reported, nothing else
  CHECK(state.get("zones")->items.size() == 4);
}

int main() {
  test_defaults();
  test_zones();
  test_panel_settings();
  test_document();
  test_state_and_commands();
  test_runner();
  test_debounce_and_tamper();
  test_server_commands();
  test_state_follows_the_panel();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
