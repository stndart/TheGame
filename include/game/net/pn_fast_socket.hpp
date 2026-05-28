#pragma once

#include "game/net/pn_layout.hpp"

// Proud::CFastSocket worker socket - GAME.exe cluster @ 0xD54xxx–0xD57xxx.
class PNFastSocket {
public:
  // w_wsasend_1 @ pn::rva::kFastSend
  int send(void *data, int len);

  // w_wsarecv_1 @ pn::rva::kFastRecv
  int recv(int len);

  // sub_D55510 - overlapped RX completion (out[5] = byte count).
  char recv_complete(unsigned char wait, std::uint32_t *out);

  // sub_D55660 - overlapped TX completion.
  char send_complete(unsigned char wait, std::uint32_t *out);

  // sub_D558A0 - RX scratch after WSARecv (+0x94 / +0x98).
  const char *recv_staging_ptr() const;

  // sub_D56170 - skip 10035 / 997; *(this+0x15)==1; log only (no game vt[1]).
  void report_error(int err, void *msg_ctx);

private:
  char *send_staging_data();
  PNGrowableBuffer &send_staging();
  PNGrowableBuffer &recv_growable();
};
