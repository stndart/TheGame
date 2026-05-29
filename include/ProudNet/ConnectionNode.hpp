#pragma once

#include "game/engine/String.h"
#include "ProudNet/Layout.hpp"

namespace Proud {

// Outbound slice + arm @ sub_D6C880 (lives at conn+4).
struct SendArm {
  CFastSocket **fast_socket;       // conn+4
  CFastSocket **owner_fast_socket; // conn+8 (ConnectionNode::fast_socket)
  String outbound;               // conn+12 (conn::kSendString)
  std::uint8_t send_pending;
  std::uint8_t _pad[3];
  int send_offset;

  bool arm_send();
};

// Per-connection ProudNet TCP worker node (sub_D6F7B0).
struct ConnectionNode {
  std::uint8_t _head[conn::kFastSocket];
  CFastSocket **fast_socket; // *(conn+8) indirection per IDA
  std::uint8_t _pad_to_recv[conn::kRecvBuffer - conn::kFastSocket -
                            sizeof(CFastSocket *)];
  RecvBuffer recv;
  std::uint8_t _pad_to_state[conn::kFsmState - conn::kRecvBuffer -
                             sizeof(RecvBuffer)];
  int fsm_state;

  SendArm &send_arm();
  CFastSocket *fast() const;

  char run_state_1();
  char run_state_2();
  int run_state_3();

private:
  int outbound_remaining() const;
};

} // namespace Proud
