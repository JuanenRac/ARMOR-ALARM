// ARMOR-NODE-ALARM - host tests of how the node is wired from its settings (core/alarm_wiring.hpp): how each zone is read, when the converter and the Zigbee radio are the node's, which topics it
// listens to and which relay is the siren; and that these decisions agree with what the settings check (core/alarm_config.hpp) lets through. No hardware was used.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>

#include "../core/alarm_wiring.hpp"

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
using alarm::ZoneInput;

// The relays of the base board by name, as relay_outputs::index_of finds them: the place among the enabled ones, or -1.
static int relay_place(const config::Settings& s, const std::string& name) {
  int place = 0;
  for (const config::RelaySetting& relay : s.relays) {
    if (!relay.enabled) continue;
    if (relay.name == name) return place;
    ++place;
  }
  return -1;
}

static config::Settings base() {
  config::Settings s = config::default_settings("a1b2c3");
  s.node_id = "alarm-1";
  return s;
}

static void test_nothing_configured() {
  const config::Settings s = base();
  const alarm::Wiring w = alarm::plan_wiring(s, [&](const std::string& n) { return relay_place(s, n); });
  for (const ZoneInput z : w.zones) CHECK(z == ZoneInput::kOff);
  for (const bool b : w.buttons) CHECK(!b);
  CHECK(!w.needs_converter && !w.takes_radio && w.radio_after_s == 0 && !w.listens_command && w.listens_zone_feed && w.listens_policy);
  CHECK(w.siren_relay == -1 && !w.siren_relay_unknown && !w.reserve_siren_relay);
  const std::vector<std::string> topics = w.topics("alarm-1");
  CHECK(topics.size() == 2 && topics[0] == alarm::topic("alarm-1", "zigbee") && topics[1] == alarm::topic("alarm-1", "policy"));
}

static void test_how_a_zone_is_read() {
  config::Settings s = base();
  s.zones[0].enabled = true; s.zones[0].input = "loop"; s.zones[0].pin = 1;
  s.zones[1].enabled = true; s.zones[1].input = "digital"; s.zones[1].pin = 4;
  s.zones[2].enabled = true; s.zones[2].zigbee = "00124b0001020304";
  s.zones[3].enabled = false; s.zones[3].input = "loop";
  const alarm::Wiring w = alarm::plan_wiring(s, [](const std::string&) { return -1; });
  CHECK(w.zones[0] == ZoneInput::kAnalogLoop && w.zones[1] == ZoneInput::kDigital && w.zones[2] == ZoneInput::kZigbee && w.zones[3] == ZoneInput::kOff);
  CHECK(w.needs_converter);
  CHECK(w.zones_read(ZoneInput::kAnalogLoop) == 1 && w.zones_read(ZoneInput::kDigital) == 1 && w.zones_read(ZoneInput::kZigbee) == 1);
  CHECK(std::string(alarm::to_text(ZoneInput::kAnalogLoop)) == "loop" && std::string(alarm::to_text(ZoneInput::kZigbee)) == "zigbee");
  // a Zigbee sensor wins over the input kind (a zone with an address has no pin)
  config::Settings z = base();
  z.zones[0].enabled = true; z.zones[0].input = "digital"; z.zones[0].zigbee = "00124b0001020304";
  CHECK(alarm::plan_wiring(z, [](const std::string&) { return -1; }).zones[0] == ZoneInput::kZigbee);
  // the converter is the node's only for loops: contacts and Zigbee sensors do not start it
  config::Settings d = base();
  d.zones[0].enabled = true; d.zones[0].input = "digital"; d.zones[0].pin = 4;
  d.zones[1].enabled = true; d.zones[1].zigbee = "00124b0001020304";
  CHECK(!alarm::plan_wiring(d, [](const std::string&) { return -1; }).needs_converter);
  // a disabled loop does not start it either
  config::Settings off = base();
  off.zones[0].enabled = false; off.zones[0].input = "loop"; off.zones[0].pin = 1;
  CHECK(!alarm::plan_wiring(off, [](const std::string&) { return -1; }).needs_converter);
}

static void test_buttons() {
  config::Settings s = base();
  s.buttons[1].enabled = true; s.buttons[1].pin = 4;
  s.buttons[3].enabled = true; s.buttons[3].pin = 5;
  const alarm::Wiring w = alarm::plan_wiring(s, [](const std::string&) { return -1; });
  CHECK(!w.buttons[0] && w.buttons[1] && !w.buttons[2] && w.buttons[3]);
}

static void test_the_radio() {
  const auto takes = [](config::Settings s) { return alarm::plan_wiring(s, [](const std::string&) { return -1; }); };
  config::Settings s = base();
  CHECK(!takes(s).takes_radio);                                         // never by default
  s.zigbee_local.after_s = 60;
  CHECK(!takes(s).takes_radio && takes(s).radio_after_s == 0);          // wanted, but there is no radio at all
  s.zigbee.enabled = true;
  CHECK(takes(s).takes_radio && takes(s).radio_after_s == 60);          // the radio on the board
  s.zigbee.enabled = false; s.zigbee.network_host = "192.168.0.50";
  CHECK(takes(s).takes_radio && takes(s).radio_after_s == 60);          // a coordinator on the network
  s.zigbee_local.after_s = 0;
  CHECK(!takes(s).takes_radio);                                         // 0 is never, whatever radio there is
}

static void test_the_topics() {
  config::Settings s = base();
  s.server.commands = true;
  const alarm::Wiring w = alarm::plan_wiring(s, [](const std::string&) { return -1; });
  const std::vector<std::string> topics = w.topics("alarm-1");
  CHECK(w.listens_command && topics.size() == 3);
  CHECK(topics[0] == "armor/alarm/alarm-1/zigbee" && topics[1] == "armor/alarm/alarm-1/policy" && topics[2] == "armor/alarm/alarm-1/command");
  s.server.commands = false;
  const std::vector<std::string> quiet = alarm::plan_wiring(s, [](const std::string&) { return -1; }).topics("alarm-1");
  CHECK(quiet.size() == 2);
  for (const std::string& t : quiet) CHECK(t.find("command") == std::string::npos);   // a node the server may not command never listens to the command topic
}

static void test_the_siren_relay() {
  config::Settings s = base();
  config::RelaySetting a, b;
  a.enabled = true; a.name = "light";
  b.enabled = true; b.name = "siren";
  s.relays[0] = a; s.relays[1] = b;
  s.panel.siren_relay = "siren";
  alarm::Wiring w = alarm::plan_wiring(s, [&](const std::string& n) { return relay_place(s, n); });
  CHECK(w.siren_relay == 1 && !w.siren_relay_unknown && w.reserve_siren_relay);
  s.panel.siren_relay = "gate";
  w = alarm::plan_wiring(s, [&](const std::string& n) { return relay_place(s, n); });
  CHECK(w.siren_relay == -1 && w.siren_relay_unknown && !w.reserve_siren_relay);   // named, not there: no siren, and no relay is taken from anyone
  s.panel.siren_relay = "";
  w = alarm::plan_wiring(s, [&](const std::string& n) { return relay_place(s, n); });
  CHECK(w.siren_relay == -1 && !w.siren_relay_unknown && !w.reserve_siren_relay);
  // a relay that is not enabled is not the siren
  s.relays[1].enabled = false;
  s.panel.siren_relay = "siren";
  w = alarm::plan_wiring(s, [&](const std::string& n) { return relay_place(s, n); });
  CHECK(w.siren_relay_unknown && !w.reserve_siren_relay);
}

// What the settings check lets through and what the wiring does agree: the wiring is never surprised by a document that was accepted.
static void test_the_wiring_agrees_with_the_settings_check() {
  const auto problem = [](const config::Settings& s, const char* path) {
    for (const auto& p : config::validate(s)) if (p.path == path) return true;
    return false;
  };
  config::Settings s = base();
  s.panel.pin = "1234";
  s.zones[0].enabled = true; s.zones[0].id = "kitchen"; s.zones[0].zigbee = "00124b0001020304";
  s.zigbee.enabled = true;
  s.zigbee_local.after_s = 0;
  CHECK(problem(s, "zones.0.zigbee"));                                  // a Zigbee zone with no radio of its own is refused ...
  CHECK(!alarm::plan_wiring(s, [](const std::string&) { return -1; }).takes_radio);   // ... which is what the wiring would have done: nothing reads that zone
  s.zigbee_local.after_s = 30;
  CHECK(!problem(s, "zones.0.zigbee") && !problem(s, "zigbee_local.after_s"));
  CHECK(alarm::plan_wiring(s, [](const std::string&) { return -1; }).takes_radio);    // ... and accepted once the node can take the radio
  // a siren relay the settings check refuses is the one the wiring finds missing, and the other way round
  config::RelaySetting r;
  r.enabled = true; r.name = "siren";
  s.relays[0] = r;
  for (const char* named : {"", "siren", "gate"}) {
    s.panel.siren_relay = named;
    const alarm::Wiring w = alarm::plan_wiring(s, [&](const std::string& n) { return relay_place(s, n); });
    CHECK(problem(s, "panel.siren_relay") == w.siren_relay_unknown);
  }
  // commands on without a broker are refused, and the wiring listens to the topic only when commands are on
  s.panel.siren_relay = "";
  s.server.commands = true; s.mqtt.enabled = false;
  CHECK(problem(s, "server.commands"));
}

int main() {
  test_nothing_configured();
  test_how_a_zone_is_read();
  test_buttons();
  test_the_radio();
  test_the_topics();
  test_the_siren_relay();
  test_the_wiring_agrees_with_the_settings_check();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
