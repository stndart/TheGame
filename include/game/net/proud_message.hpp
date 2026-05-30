#pragma once

#include <cstddef>
#include <cstdint>

// Inner Proud payload after TCP framing (opcode byte + optional RMI id + body).
namespace ProudMessage {

enum : uint8_t {
  kOpHello = 0x2F,
  kOpMessage = 0x02,
  kOpKeyBlob = 0x04,
};

enum : uint16_t {
  kRmiNetUserConnectRes = 0x3F3E,
};

struct Summary {
  uint8_t opcode{0};
  uint16_t rmi_id{0};
  bool has_rmi{false};
  size_t body_len{0};
};

inline Summary summarize_payload(const uint8_t *data, size_t len) {
  Summary s{};
  if (!data || len == 0)
    return s;
  s.opcode = data[0];
  if (s.opcode == kOpMessage && len >= 3) {
    s.has_rmi = true;
    s.rmi_id = static_cast<uint16_t>(data[1]) |
               (static_cast<uint16_t>(data[2]) << 8);
    s.body_len = len - 3;
  } else {
    s.body_len = len - 1;
  }
  return s;
}

} // namespace ProudMessage
