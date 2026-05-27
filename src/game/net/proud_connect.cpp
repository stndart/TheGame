#include "game/net/proud_connect.hpp"

#include "console.h"
#include "game/server_override.hpp"
#include "system_hooks.h"

#include <cstring>
#include <winsock2.h>

bool ProudConnect::Socket::w_connect_3() {
  auto *base = reinterpret_cast<unsigned char *>(this);
  const SOCKET s = *reinterpret_cast<SOCKET *>(base + 20);

  sockaddr_in peer{};
  memcpy(&peer, base + 4, sizeof(sockaddr_in));

  if (peer.sin_family == AF_INET) {
    const u_short port = ntohs(peer.sin_port);
    in_addr was = peer.sin_addr;
    if (ServerOverride::remap_sockaddr_in(&peer) &&
        port == ServerOverride::kGameLegPort)
      logf("w_connect_3:27380 %s -> %s", inet_ntoa(was), inet_ntoa(peer.sin_addr));
  }

  return connect_syshandle(s, reinterpret_cast<sockaddr *>(&peer), 16) !=
         SOCKET_ERROR;
}
