#pragma once

// Proud TCP framing (GAME sub_D84BB0 recv / send @ sub_D84970 cluster).
// Wire format: include/game/net/proud_frame.hpp

class PNTcpFrame {
public:
  // sub_D84BB0 — __thiscall; caller cleans stack (ret, not ret 4).
  char extract(void *stream_ctx);

  // Send framer entry (E8 targets 0xD84910, inside sub_D84970) — __thiscall; ret 8.
  void frame_send(void *arg0, void *arg1);
};
