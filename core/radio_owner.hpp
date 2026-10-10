// ARMOR-ALARM - who holds the Zigbee radio: the server's client, or the node itself once that client has been away long enough. No hardware.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The radio's serial line is lent to one TCP client (the server's Zigbee2MQTT). While a client is connected it is the owner and the node only carries bytes. When no client has been
// connected for `after_ms` the node becomes the owner and speaks Zigbee itself; the moment a client connects again the node lets go. After 0 the node never takes it.
#pragma once
#include <cstdint>

namespace armor::zigbee {

class RadioOwner {
 public:
  explicit RadioOwner(std::uint64_t after_ms, std::uint64_t started_ms = 0) : after_ms_(after_ms), away_since_ms_(started_ms) {}

  enum class Owner { kClient, kNode };

  // Call every turn with whether a client is connected now; it says who holds the radio.
  Owner update(bool client_connected, std::uint64_t now_ms) {
    if (client_connected) { away_since_ms_ = now_ms; owner_ = Owner::kClient; return owner_; }
    if (after_ms_ != 0 && now_ms - away_since_ms_ >= after_ms_) {
      if (owner_ != Owner::kNode) ++takeovers_;
      owner_ = Owner::kNode;
    }
    return owner_;
  }
  Owner owner() const { return owner_; }
  std::uint32_t takeovers() const { return takeovers_; }

 private:
  std::uint64_t after_ms_;
  std::uint64_t away_since_ms_;
  Owner owner_ = Owner::kClient;
  std::uint32_t takeovers_ = 0;
};

}  // namespace armor::zigbee
