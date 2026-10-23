// ARMOR-NODE-ALARM - the security options the server keeps for the panel, and what the node does with them: the hash that ties the saved armed state to the zones and the PIN it was saved with, and the
// checks of the server's zone messages (the signature and the sequence number). Pure: no hardware, all tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The server tells the panel its options on armor/alarm/<node>/policy ({"config_hash":..,"zone_feed":..,"zone_feed_signature":..} and, when the administrator did not leave it at "if_set",
// "disarm_pin": "never" | "always", ARMOR-COMMON's alarm_policy). Every option can also be fixed on the node (server.config_hash, server.zone_feed, server.zone_signature: "policy", "on" or "off";
// server.disarm_pin: "policy", "never", "if_set" or "always"). What the node does when the server has never spoken is what it always did: restore the saved state
// without a hash, take the zone messages, accept an unsigned one.
#pragma once
#include <cstdint>
#include <string>

#include "alarm_config.hpp"
#include "alarm_message.hpp"
#include "hmac_sha256.hpp"
#include "json.hpp"

namespace armor::alarm {

// What is asked of whoever disarms the panel or silences its siren from the panel's own page or the central's API. A panel with zones always has a PIN (the configuration refuses zones without
// one), so "if_set" and "always" ask for it exactly as the panel always did; "never" asks for none, counts no wrong PIN and so locks nobody out (the person must still be signed in as an administrator).
enum class DisarmPin { kIfSet, kNever, kAlways };
inline const char* disarm_pin_text(DisarmPin v) { return v == DisarmPin::kNever ? "never" : v == DisarmPin::kAlways ? "always" : "if_set"; }
inline bool parse_disarm_pin(const std::string& text, DisarmPin& out) {
  if (text == "never") out = DisarmPin::kNever;
  else if (text == "if_set") out = DisarmPin::kIfSet;
  else if (text == "always") out = DisarmPin::kAlways;
  else return false;
  return true;
}

struct Policy {
  DisarmPin disarm_pin = DisarmPin::kIfSet;   // what the server last said; absent in its message means "if_set"
  bool config_hash = false;       // not tied to the configuration until the server says so
  bool zone_feed = true;
  bool zone_feed_signature = false;
  bool seal_secrets = false;      // the server asks the node to keep the secrets it stores sealed (nodes.nvs_encryption "secrets"); absent in its message means off
  bool heard = false;             // the server has said it (or it was read back from flash)
};

// The server's message, strictly: the three options each a true/false and, optionally, "disarm_pin" with one of its three words (left out when it is "if_set", so a message of the older form is the
// same as one that says "if_set") and "nvs_encryption" with the word "secrets" (left out while the option is off). Anything else changes nothing.
inline bool parse_policy(const std::string& text, Policy& out) {
  json::Value document;
  if (text.size() > 256 || !json::parse(text, document) || !document.is_object()) return false;
  const json::Value* pin = document.get("disarm_pin");
  const json::Value* encryption = document.get("nvs_encryption");
  if (document.names.size() != 3u + (pin != nullptr ? 1u : 0u) + (encryption != nullptr ? 1u : 0u)) return false;
  const json::Value* hash = document.get("config_hash");
  const json::Value* feed = document.get("zone_feed");
  const json::Value* signature = document.get("zone_feed_signature");
  if (hash == nullptr || !hash->is_bool() || feed == nullptr || !feed->is_bool() || signature == nullptr || !signature->is_bool()) return false;
  DisarmPin disarm_pin = DisarmPin::kIfSet;
  if (pin != nullptr && (!pin->is_string() || !parse_disarm_pin(pin->text, disarm_pin))) return false;
  if (encryption != nullptr && (!encryption->is_string() || encryption->text != "secrets")) return false;
  out.seal_secrets = encryption != nullptr;
  out.config_hash = hash->boolean;
  out.zone_feed = feed->boolean;
  out.zone_feed_signature = signature->boolean;
  out.disarm_pin = disarm_pin;
  out.heard = true;
  return true;
}

// The canonical form kept in flash: the message of the older form while "disarm_pin" is "if_set", so a panel updated with nothing new to keep writes nothing new.
inline std::string policy_text(const Policy& policy) {
  json::Writer w;
  w.begin_object().field("config_hash", policy.config_hash).field("zone_feed", policy.zone_feed).field("zone_feed_signature", policy.zone_feed_signature);
  if (policy.disarm_pin != DisarmPin::kIfSet) w.field("disarm_pin", disarm_pin_text(policy.disarm_pin));
  if (policy.seal_secrets) w.field("nvs_encryption", "secrets");
  w.end_object();
  return w.str();
}

// What the node does now: the server's last word where the node leaves the option to it, the node's own choice where it fixed it.
struct Effective {
  bool config_hash = false, zone_feed = true, signature_required = false;
  DisarmPin disarm_pin = DisarmPin::kIfSet;
  bool pin_asked() const { return disarm_pin != DisarmPin::kNever; }
};
inline Effective effective(const config::ServerLink& link, const Policy& policy) {
  Effective out;
  out.disarm_pin = link.disarm_pin == config::PinChoice::kPolicy ? policy.disarm_pin : link.disarm_pin == config::PinChoice::kNever ? DisarmPin::kNever : link.disarm_pin == config::PinChoice::kAlways ? DisarmPin::kAlways : DisarmPin::kIfSet;
  out.config_hash = link.config_hash == config::Choice::kPolicy ? policy.config_hash : link.config_hash == config::Choice::kOn;
  out.zone_feed = link.zone_feed == config::Choice::kPolicy ? policy.zone_feed : link.zone_feed == config::Choice::kOn;
  // The server's wish for signed messages binds a panel only once it holds the key to check them with: before that the panel cannot tell a good signature from a bad one, and refusing the whole feed
  // would leave its Zigbee zones silent (and, armed, in tamper) after an update. A requirement fixed on the node itself ("on") needs the key to be stored (config validate()).
  out.signature_required = link.zone_signature == config::Choice::kPolicy ? (policy.zone_feed_signature && !link.zone_feed_key.empty()) : link.zone_signature == config::Choice::kOn;
  return out;
}

// ---- the hash of the configuration ---------------------------------------------------------------------------------------------

// What the saved armed state depends on: the zones (which are guarded, where, and how) and the PIN. Sixteen bytes of SHA-256 as hexadecimal; the PIN itself is not recoverable from it in any
// practical way beyond what the flash already holds, and it is kept next to the state it protects.
inline std::string config_fingerprint(const config::Settings& s) {
  std::string text = "armor-alarm-config-v1|" + s.node_id + "|" + s.panel.pin + "|";
  for (const config::ZoneSetting& zone : s.zones) {
    if (!zone.enabled) { text += "-|"; continue; }
    text += zone.id + "," + std::to_string(zone.pin) + "," + zone.input + "," + zone.contact + "," + zone.role + "," + zone.zigbee + "," + kind_text(zone.kind) + "," + std::to_string(zone.hold_ms) + "|";
  }
  return hash::to_hex(hash::sha256(text).data(), 16);
}

// Whether the saved armed state may come back: always unless the option is on and the state was saved under another configuration. A state saved before the option (no stored hash) comes back too:
// there is nothing to compare, and the panel would otherwise disarm itself after an update.
inline bool may_restore(bool config_hash_on, const std::string& stored_hash, const std::string& current_hash) {
  if (!config_hash_on || stored_hash.empty()) return true;
  return hash::same_text(stored_hash, current_hash);
}

// ---- the zone messages ---------------------------------------------------------------------------------------------------------

// The HMAC-SHA256 the server puts in `sig`: over node_id|zone|state|battery_low (1 or 0)|seq with the key of the panel (ARMOR-COMMON's zone_feed_signature).
inline std::string zone_feed_signature(const std::string& key_hex, const std::string& node_id, const ZoneFeed& feed) {
  std::string key;
  if (!hash::from_hex(key_hex, key) || key.size() != 32) return std::string();
  const char* state = feed.state == ZoneState::kTriggered ? "triggered" : feed.state == ZoneState::kTamper ? "tamper" : "normal";
  const std::string text = node_id + "|" + feed.zone_id + "|" + state + "|" + (feed.battery_low ? "1" : "0") + "|" + std::to_string(feed.seq);
  return hash::to_hex(hash::hmac_sha256(reinterpret_cast<const std::uint8_t*>(key.data()), key.size(), text));
}

enum class FeedVerdict { kAccepted, kOff, kUnsigned, kNoKey, kBadSignature, kReplay, kMalformed };
inline const char* verdict_text(FeedVerdict v) {
  switch (v) {
    case FeedVerdict::kAccepted: return "accepted";
    case FeedVerdict::kOff: return "off";
    case FeedVerdict::kUnsigned: return "unsigned";
    case FeedVerdict::kNoKey: return "no_key";
    case FeedVerdict::kBadSignature: return "bad_signature";
    case FeedVerdict::kReplay: return "replay";
    case FeedVerdict::kMalformed: return "malformed";
  }
  return "malformed";
}

// Decides whether a zone message of the server counts. `last_seq` is the highest sequence number accepted so far; it is raised here when the message is taken.
//   * the feed is off (the option, or no zone at all wants it)       -> nothing counts;
//   * a signed message and the node holds the key                    -> the signature must be right and `seq` above the last one, always, signed-required or not;
//   * a signed message and the node holds no key                     -> counted only when a signature is not required (it cannot be checked);
//   * an unsigned message                                            -> counted only when a signature is not required.
inline FeedVerdict check_zone_feed(const Effective& now, const config::ServerLink& link, const std::string& node_id, const ZoneFeed& feed, std::uint64_t& last_seq) {
  if (!now.zone_feed) return FeedVerdict::kOff;
  const bool have_key = !link.zone_feed_key.empty();
  if (!feed.signed_message) return now.signature_required ? FeedVerdict::kUnsigned : FeedVerdict::kAccepted;
  if (!have_key) return now.signature_required ? FeedVerdict::kNoKey : FeedVerdict::kAccepted;
  const std::string expected = zone_feed_signature(link.zone_feed_key, node_id, feed);
  if (expected.empty() || !hash::same_text(expected, feed.sig)) return FeedVerdict::kBadSignature;
  if (feed.seq <= last_seq) return FeedVerdict::kReplay;
  last_seq = feed.seq;
  return FeedVerdict::kAccepted;
}

}  // namespace armor::alarm
