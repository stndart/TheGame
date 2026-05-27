#include "game/net/pn_select.hpp"

#include "game/net/socket_trace.hpp"
#include "game/server_override.hpp"

#include <winsock2.h>

namespace {

constexpr std::uintptr_t kFastSocketHandleOff = 300; // CFastSocket+0x12C
constexpr std::uintptr_t kReadFdSetOff = 0x104;
constexpr std::uintptr_t kExceptFdSetOff = 0x208;

} // namespace

char PNSelectContext::poll(void *fast_socket_ctx, std::uint32_t *out) {
  SocketTrace::note_fast_socket_ctx(fast_socket_ctx);
  if (ServerOverride::override_active() && fast_socket_ctx)
    SocketTrace::sync_fast_socket_handle(fast_socket_ctx);

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(static_cast<char *>(fast_socket_ctx) +
                                    kFastSocketHandleOff);
  auto *base = reinterpret_cast<char *>(this);
  auto *read_set = reinterpret_cast<fd_set *>(base + kReadFdSetOff);
  auto *except_set = reinterpret_cast<fd_set *>(base + kExceptFdSetOff);

  if (__WSAFDIsSet(sock, read_set)) {
    *out = 0;
    return 1;
  }

  if (__WSAFDIsSet(sock, except_set)) {
    int optval = 0;
    int optlen = sizeof(optval);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&optval),
               &optlen);
    *out = static_cast<std::uint32_t>(optval);
    return 1;
  }

  return 0;
}
