// ARMOR-ALARM - the Zigbee radio of the base board: the serial line lent to one client of the network over TCP.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/io_config.hpp"
#include "core/zigbee_bridge.hpp"

namespace armor::zigbee_bridge {

// Opens the radio's UART (UART2) and listens on the TCP port when the settings turn the bridge on; does nothing otherwise. Call it once the network is up.
void start(const config::IoSettings& settings);

zigbee::Status status();

// The node itself as a Zigbee host, for the moments when the radio is its own (the server's client has been away for `after_s` seconds; 0: never). The bridge's task calls these and no
// others: `begin` when the node takes the radio, `end` when a client connects again, `feed` with what the radio says, `tick` every turn, `take_output` for what is to be written to it.
class LocalHost {
 public:
  virtual ~LocalHost() = default;
  virtual void begin(std::uint64_t now_ms) = 0;
  virtual void end() = 0;
  virtual void feed(const std::uint8_t* bytes, std::size_t count, std::uint64_t now_ms) = 0;
  virtual void tick(std::uint64_t now_ms) = 0;
  virtual std::size_t take_output(std::uint8_t* out, std::size_t capacity) = 0;
};
// Call before start().
void set_local_host(LocalHost* host, unsigned after_s);
// True while the node holds the radio.
bool local_active();

// Resets the radio (RESET_N low, then let go), or resets it into its serial boot loader to be given a firmware. False when the bridge is off or the line is not wired.
bool reset_radio(bool bootloader);

}  // namespace armor::zigbee_bridge
