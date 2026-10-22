// ARMOR-ALARM - SHA-256 and HMAC-SHA256, small and portable: the same code runs on the board and in the tests on a computer, so the signature of the server's zone messages and the hash of
// the panel's configuration are checked against known values before they are ever run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace armor::hash {

using Digest = std::array<std::uint8_t, 32>;

class Sha256 {
 public:
  Sha256() { reset(); }
  void reset() {
    state_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    length_ = 0;
    used_ = 0;
  }
  void update(const std::uint8_t* data, std::size_t size) {
    length_ += size;
    while (size > 0) {
      const std::size_t take = std::min<std::size_t>(size, block_.size() - used_);
      for (std::size_t i = 0; i < take; ++i) block_[used_ + i] = data[i];
      used_ += take; data += take; size -= take;
      if (used_ == block_.size()) { compress(); used_ = 0; }
    }
  }
  void update(std::string_view text) { update(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()); }
  Digest finish() {
    const std::uint64_t bits = static_cast<std::uint64_t>(length_) * 8u;
    const std::uint8_t one = 0x80, zero = 0;
    update(&one, 1);
    while (used_ != 56) update(&zero, 1);
    std::uint8_t tail[8];
    for (int i = 0; i < 8; ++i) tail[i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
    update(tail, 8);
    Digest out{};
    for (std::size_t i = 0; i < 8; ++i) for (int j = 0; j < 4; ++j) out[i * 4 + static_cast<std::size_t>(j)] = static_cast<std::uint8_t>(state_[i] >> (24 - 8 * j));
    reset();
    return out;
  }

 private:
  static std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
  void compress() {
    static constexpr std::uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    std::uint32_t w[64];
    for (std::size_t i = 0; i < 16; ++i) w[i] = (static_cast<std::uint32_t>(block_[i * 4]) << 24) | (static_cast<std::uint32_t>(block_[i * 4 + 1]) << 16) | (static_cast<std::uint32_t>(block_[i * 4 + 2]) << 8) | block_[i * 4 + 3];
    for (std::size_t i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3], e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (std::size_t i = 0; i < 64; ++i) {
      const std::uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d; state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
  }
  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, 64> block_{};
  std::size_t used_ = 0;
  std::uint64_t length_ = 0;
};

inline Digest sha256(std::string_view text) {
  Sha256 hash;
  hash.update(text);
  return hash.finish();
}

inline Digest hmac_sha256(const std::uint8_t* key, std::size_t key_size, std::string_view message) {
  std::array<std::uint8_t, 64> pad{};
  if (key_size > pad.size()) {
    Sha256 shrink;
    shrink.update(key, key_size);
    const Digest shrunk = shrink.finish();
    for (std::size_t i = 0; i < shrunk.size(); ++i) pad[i] = shrunk[i];
  } else {
    for (std::size_t i = 0; i < key_size; ++i) pad[i] = key[i];
  }
  std::array<std::uint8_t, 64> inner = pad, outer = pad;
  for (std::size_t i = 0; i < pad.size(); ++i) { inner[i] ^= 0x36; outer[i] ^= 0x5c; }
  Sha256 first;
  first.update(inner.data(), inner.size());
  first.update(message);
  const Digest inner_digest = first.finish();
  Sha256 second;
  second.update(outer.data(), outer.size());
  second.update(inner_digest.data(), inner_digest.size());
  return second.finish();
}

inline std::string to_hex(const std::uint8_t* data, std::size_t size) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(size * 2);
  for (std::size_t i = 0; i < size; ++i) { out += digits[data[i] >> 4]; out += digits[data[i] & 15]; }
  return out;
}
inline std::string to_hex(const Digest& digest) { return to_hex(digest.data(), digest.size()); }

// Lowercase hexadecimal text to bytes; false for anything else (odd length, a capital letter, a stray character).
inline bool from_hex(std::string_view text, std::string& out) {
  if (text.size() % 2 != 0) return false;
  out.clear();
  auto nibble = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  for (std::size_t i = 0; i < text.size(); i += 2) {
    const int high = nibble(text[i]), low = nibble(text[i + 1]);
    if (high < 0 || low < 0) return false;
    out += static_cast<char>(high * 16 + low);
  }
  return true;
}

// The same answer whatever the content: the time does not say how many leading characters matched.
inline bool same_text(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  unsigned char difference = 0;
  for (std::size_t i = 0; i < a.size(); ++i) difference = static_cast<unsigned char>(difference | (static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i])));
  return difference == 0;
}

}  // namespace armor::hash
