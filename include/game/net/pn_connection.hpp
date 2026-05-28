#pragma once

#include "game/engine/String.h"
#include "game/net/pn_layout.hpp"

// Outbound slice + arm @ sub_D6C880 (lives at conn+4).
struct PNSendArm {
  PNFastSocket **fast_socket;       // conn+4
  PNFastSocket **owner_fast_socket; // conn+8 (PNConnectionNode::fast_socket)
  String outbound; // conn+12 (pn::conn::kSendString); game String object layout
  std::uint8_t send_pending;
  std::uint8_t _pad[3];
  int send_offset;

  bool arm_send();
};

// Per-connection ProudNet TCP worker node (sub_D6F7B0).
struct PNConnectionNode {
  std::uint8_t _head[pn::conn::kFastSocket];
  PNFastSocket **fast_socket; // *(conn+8) indirection per IDA
  std::uint8_t _pad_to_recv[pn::conn::kRecvBuffer - pn::conn::kFastSocket -
                            sizeof(PNFastSocket *)];
  PNRecvBuffer recv;
  std::uint8_t _pad_to_state[pn::conn::kFsmState - pn::conn::kRecvBuffer -
                             sizeof(PNRecvBuffer)];
  int fsm_state;

  PNSendArm &send_arm();
  PNFastSocket *fast() const;

  char run_state_1();
  char run_state_2();
  int run_state_3();

private:
  int outbound_remaining() const;
};
