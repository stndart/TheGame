#pragma once

#include "game/net/proud_message.hpp"

#include <cstddef>
#include <cstdint>

// Scan a raw TCP chunk for complete Proud frames (magic 13 57 + varint length).
namespace ProudFrameParse {

struct ParsedFrame {
  size_t wire_bytes{0};
  size_t payload_len{0};
  ProudMessage::Summary inner{};
};

inline size_t read_length_le(const uint8_t *p, uint8_t width) {
  if (width == 1)
    return p[0];
  if (width == 2)
    return static_cast<size_t>(p[0]) | (static_cast<size_t>(p[1]) << 8);
  return static_cast<size_t>(p[0]) | (static_cast<size_t>(p[1]) << 8) |
         (static_cast<size_t>(p[2]) << 16) |
         (static_cast<size_t>(p[3]) << 24);
}

// Append complete frames found in [data, data+len). Stops at first incomplete frame.
inline size_t scan_frames(const uint8_t *data, size_t len, ParsedFrame *out,
                          size_t out_cap) {
  if (!data || !out || out_cap == 0)
    return 0;

  size_t count = 0;
  size_t off = 0;

  while (off + 3 <= len && count < out_cap) {
    if (data[off] != 0x13 || data[off + 1] != 0x57) {
      ++off;
      continue;
    }

    const uint8_t len_tag = data[off + 2];
    if (len_tag != 1 && len_tag != 2 && len_tag != 4) {
      ++off;
      continue;
    }

    const size_t hdr = 3 + len_tag;
    if (off + hdr > len)
      break;

    const size_t payload_len =
        read_length_le(data + off + 3, len_tag);
    const size_t wire = 2 + 1 + len_tag + payload_len;
    if (off + wire > len)
      break;

    ParsedFrame &f = out[count++];
    f.wire_bytes = wire;
    f.payload_len = payload_len;
    f.inner = ProudMessage::summarize_payload(data + off + hdr, payload_len);
    off += wire;
  }

  return count;
}

inline size_t trailing_unparsed(const uint8_t *data, size_t len,
                               size_t parsed_wire_bytes) {
  if (!data || parsed_wire_bytes >= len)
    return 0;
  return len - parsed_wire_bytes;
}

} // namespace ProudFrameParse
