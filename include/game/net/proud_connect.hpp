#pragma once

namespace ProudConnect {

// Opaque ProudNet object passed as `this` to w_connect_3 @ 0x014F6DC0.
class Socket {
public:
  // sockaddr_in at +4, SOCKET at +20.
  bool w_connect_3();
};

} // namespace ProudConnect
