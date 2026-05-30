#pragma once

#include <winsock2.h>

namespace ProudConnect {

// Opaque ProudNet object passed as `this` to w_connect_3 @ 0x014F6DC0.
class Socket {
public:
  // sockaddr_in at +4, SOCKET at +20.
  bool w_connect_3();
};

// w_connect_2 @ 0x14314E0 - alternate connect helper (__fastcall).
extern "C" int __fastcall w_connect_2(int *out_zero, SOCKET *out_socket,
                                      int *ctx, int param4);

} // namespace ProudConnect
