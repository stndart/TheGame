#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Proud TCP wire framing (sub_D84BB0 / sub_D84970): magic 0x5713 then varint length
// (sub_D9DEC0) then payload.
namespace ProudFrame {

inline constexpr uint16_t kMagic = 0x5713;

// Encode payload length using the game's tag scheme for len <= 0x7FFF.
inline std::vector<uint8_t> encode_length(size_t len) {
  std::vector<uint8_t> out;
  if (len + 128 <= 0xFF) {
    out.push_back(1);
    out.push_back(static_cast<uint8_t>(len));
  } else if (len + 0x8000 <= 0xFFFF) {
    out.push_back(2);
    out.push_back(static_cast<uint8_t>(len & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
  } else {
    out.push_back(4);
    out.push_back(static_cast<uint8_t>(len & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
  }
  return out;
}

inline std::vector<uint8_t> wrap(const uint8_t *payload, size_t len) {
  std::vector<uint8_t> frame;
  frame.reserve(2 + 5 + len);
  frame.push_back(0x13);
  frame.push_back(0x57);
  auto length_bytes = encode_length(len);
  frame.insert(frame.end(), length_bytes.begin(), length_bytes.end());
  frame.insert(frame.end(), payload, payload + len);
  return frame;
}

} // namespace ProudFrame
