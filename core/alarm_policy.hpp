// ARMOR-ALARM - the security options the server keeps for the panel, and what the node does with them: the hash that ties the saved armed state to the zones and the PIN it was saved with, and the
// checks of the server's zone messages (the signature and the sequence number). Pure: no hardware, all tested on a computer.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The server tells the panel its options on armor/alarm/<node>/policy ({"config_hash":..,"zone_feed":..,"zone_feed_signature":..}, ARMOR-COMMON's alarm_policy). Every option can also be fixed on the
// node (server.config_hash, server.zone_feed, server.zone_signature: "policy", "on" or "off"). What the node does when the server has never spoken is what it always did: restore the saved state
// without a hash, take the zone messages, accept an unsigned one.
#pragma once
#include <cstdint>
#include <string>

#include "alarm_config.hpp"
#include "alarm_message.hpp"
#include "hmac_sha256.hpp"
#include "json.hpp"

namespace armor::alarm {

struct Policy {
  bool config_hash = false;       // not tied to the configuration until the server says so
  bool zone_feed = true;
  bool zone_feed_signature = false;
  bool heard = false;             // the server has said it (or it was read back from flash)
};

// The server's message, strictly: exactly the three options and each a true/false. Anything else changes nothing.
inline bool parse_policy(const std::string& text, Policy& out) {
  json::Value document;
  if (text.size() > 256 || !json::parse(text, document) || !document.is_object() || document.names.size() != 3) return false;
  const json::Value* hash = document.get("config_hash");
  const json::Value* feed = document.get("zone_feed");
  const json::Value* signature = document.get("zone_feed_signature");
  if (hash == nullptr || !hash->is_bool() || feed == nullptr || !feed->is_bool() || signature == nullptr || !signature->is_bool()) return false;
  out.config_hash = hash->boolean;
  out.zone_feed = feed->boolean;
  out.zone_feed_signature = signature->boolean;
  out.heard = true;
  return true;
}

inline std::string policy_text(const Policy& policy) {
  json::Writer w;
  w.begin_object().field("config_hash", policy.config_hash).field("zone_feed", policy.zone_feed).field("zone_feed_signature", policy.zone_feed_signature).end_object();
  return w.str();
}

// What the node does now: the server's last word where the node leaves the option to it, the node's own choice where it fixed it.
struct Effective {
  bool config_hash = false, zone_feed = true, signature_required = false;
};
inline Effective effective(const config::ServerLink& link, const Policy& policy) {
  Effective out;
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
