#pragma once

// Opaque stand-in for PN recv accumulator at conn+0x20 (sub_D71CE0 `this`).
class PNRecvBuffer {
public:
  // sub_D71CE0 — append completed WSARecv bytes (PN FSM state 3).
  void append(const char *src, int len);
};
