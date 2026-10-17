// ARMOR-ALARM - the alarm at work: one task reads the wired zones, runs the panel (core/alarm_runner.hpp), drives the siren and tells the broker what it sees.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <array>
#include <functional>
#include <string>

#include "core/alarm_config.hpp"
#include "core/alarm_runner.hpp"

namespace armor::manager {

using Publish = std::function<void(const std::string& topic, const std::string& payload)>;

// Starts the task: reads each enabled zone's pin (ADC1), feeds the panel, drives the siren (a relay of the base board, named in the settings), publishes the state and, when the settings let
// the server command the panel, subscribes to its command topic. `publish` carries the node's messages (topic, JSON) to the broker. Call it after relay_outputs::start() and before mqtt_link::start().
void start(const config::Settings& settings, Publish publish);

// The state message as it would be published now.
std::string state_json();

// Each zone with its state and what its pin reads.
std::array<alarm::ZoneView, config::kZoneCount> zones();

// From the panel's page: arming needs no PIN, disarming does.
alarm::Refusal arm(alarm::Mode mode, bool force);
alarm::Refusal disarm(const std::string& pin);

// "" or why the zones are not being read ("adc": the converter could not be set up).
std::string error();

// True while the panel is armed, counting down or ringing: a reset by hand or from the panel's page is refused then (whoever stands at the node must not be able to wipe the alarm).
bool guarding();

// Erases what the node keeps of the panel (the saved state and the events), for a factory reset. Nothing is written again afterwards.
void forget();

}  // namespace armor::manager
