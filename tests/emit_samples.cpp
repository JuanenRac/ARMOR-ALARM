// ARMOR-ALARM - prints the messages a node would publish and answer, one per line ("armor/alarm/<node>/<leaf> <json>"), for tests/check_samples.py.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/alarm_config.hpp"
#include "../core/alarm_message.hpp"
#include "../core/alarm_runner.hpp"

using namespace armor;

static const double kClosed = 800, kOpen = 1800, kCut = 3000;   // millivolts of a loop, with the default limits

static config::Settings house() {
  config::Settings s = config::default_settings("a1b2c3");
  const char* names[] = {"front-door", "window", "hall-motion", "smoke"};
  const alarm::ZoneKind kinds[] = {alarm::ZoneKind::kEntry, alarm::ZoneKind::kInstant, alarm::ZoneKind::kInterior, alarm::ZoneKind::kAlways};
  for (std::size_t i = 0; i < 4; ++i) { s.zones[i].enabled = true; s.zones[i].id = names[i]; s.zones[i].name = std::string("Zone ") + names[i]; s.zones[i].kind = kinds[i]; }
  s.zones[3].name.clear();   // a zone without a name is told without one
  s.relays[0] = {true, "siren", "Siren", board::kRelayPins[0], false};
  s.panel.siren_relay = "siren";
  s.panel.pin = "1234";
  s.panel.timing.exit_delay_s = 10; s.panel.timing.entry_delay_s = 10; s.panel.timing.siren_max_s = 60; s.panel.timing.lockout_after = 3; s.panel.timing.lockout_s = 30;
  s.server.commands = true;
  return s;
}

static void run(alarm::Runner& node, std::uint64_t& now, std::uint64_t ms, const std::vector<double>& readings) {
  const std::uint64_t until = now + ms;
  while (now < until) {
    for (std::size_t i = 0; i < readings.size(); ++i) node.feed(i, readings[i], now);
    node.tick(now);
    now += 100;
  }
}

int main() {
  const config::Settings settings = house();
  const std::string id = settings.node_id;
  std::uint64_t now = 0;
  std::uint64_t stamp = 1000;
  const std::vector<double> quiet{kClosed, kClosed, kClosed, kClosed};

  alarm::Runner node(settings);
  const auto state = [&]() { std::printf("%s %s\n", alarm::topic(id, "state").c_str(), node.state_json(stamp += 1000, now).c_str()); };
  const auto command = [&](const std::string& body) {
    const std::string text = "{\"kind\":\"alarm_command\",\"node_id\":\"" + id + "\",\"timestamp_ms\":" + std::to_string(stamp) + ",\"command_id\":\"c0ffee0123456789\"," + body + "}";
    std::printf("%s %s\n", alarm::topic(id, "command").c_str(), text.c_str());
    const alarm::Reply reply = node.handle(text, now);
    if (reply.understood) std::printf("%s %s\n", alarm::topic(id, "result").c_str(), node.result_json(stamp += 100, reply).c_str());
  };

  run(node, now, 1000, quiet);
  state();                                                                              // disarmed, quiet
  run(node, now, 1000, {kClosed, kOpen, kClosed, kClosed});
  state();                                                                              // a zone open while disarmed
  command("\"action\":\"arm\",\"mode\":\"away\"");                                      // refused: a zone is open
  command("\"action\":\"disarm\"");                                                     // a disarm of a panel that is not armed
  command("\"action\":\"arm\",\"mode\":\"away\",\"force\":true");                       // accepted, the open zone is left out
  state();                                                                              // exit delay
  run(node, now, 11000, {kClosed, kOpen, kClosed, kClosed});
  state();                                                                              // armed with a bypassed zone
  command("\"action\":\"arm\",\"mode\":\"stay\"");                                      // refused: not disarmed
  run(node, now, 300, {kClosed, kOpen, kClosed, kCut});                                 // a smoke loop cut: tamper on an always zone
  state();                                                                              // alarm, siren
  command("\"action\":\"disarm\"");                                                     // accepted: the server may disarm
  state();
  run(node, now, 1000, quiet);
  command("\"action\":\"arm\",\"mode\":\"stay\"");
  run(node, now, 11000, quiet);
  run(node, now, 300, {kOpen, kClosed, kClosed, kClosed});                              // the entry door: entry delay
  state();
  run(node, now, 11000, {kOpen, kClosed, kClosed, kClosed});                            // nobody disarms: alarm
  state();
  for (int i = 0; i < 3; ++i) node.disarm("0", now + static_cast<std::uint64_t>(i) * 100);   // three wrong PINs: locked out
  state();
  now += 31000;
  node.tick(now);
  node.disarm("1234", now);
  state();
  return 0;
}
