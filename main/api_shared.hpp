// ARMOR-NODE-ALARM - what the panel does over HTTP: the status, the alarm, the settings and the search for Wi-Fi networks.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <string>
#include <string_view>

#include "core/alarm_config.hpp"

namespace armor::api {

std::string version_text();
std::string status_json();          // node, network, broker and the alarm, as the Overview page shows them
std::string zones_json();           // {"alarm":{...},"zones":[...],"catalog":[...]}: the alarm, each zone with what its pin reads, and the pins the board offers
// What a central that holds the node's token reads: {"node_id":..,"alarm":{..},"zones":[{id,state,..}],"relays":[..]}.
std::string remote_state_json();
std::string io_json();              // {"zigbee":{...},"relays":[...],"remote":{...}}: the Zigbee bridge and the relay outputs of the base board
std::string config_get_json();      // {"config":{...},"channel_auto":n,"firmware":"x.y.z"}: no password ever leaves the node
// The same document the flash keeps, secrets and all: an admin downloading the node's whole configuration to load onto an identical
// unit (manufacturing a batch of nodes), not something the ordinary panel pages ever call.
std::string config_export_json();
std::string problems_json(const config::Problems& problems);   // [{"path":..,"code":..}]

enum class PutResult { kSaved, kInvalid, kStorage };
// Applies a (partial) settings document on top of the stored one: checked in full, and stored only when nothing is wrong.
PutResult put_config(std::string_view document, config::Problems& problems, bool& restart_required);

// The Wi-Fi networks in range: {"networks":[{"ssid","rssi","channel","security"}]}, strongest first. False, with a code, when the radio is busy.
bool wifi_scan_json(std::string& data, std::string& error);

}  // namespace armor::api
