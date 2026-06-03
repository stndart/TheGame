#include "ProudNet/SocketError.hpp"

#include <winsock2.h>

#include "ProudNet/Layout.hpp"
#include "game/net/socket_trace.hpp"
#include "thegame/log.hpp"

using thegame::LogMessage;
using thegame::logp;

namespace {

void log_socket_error(void *socket_obj, int err) {
  auto *base = reinterpret_cast<char *>(socket_obj);
  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + Proud::FastSocketLayout::kSocket);
  const u_short port = SocketTrace::peer_port(sock);
  logp(sock, LogMessage("socket_error fast={} err={} port={}",
                        reinterpret_cast<void *>(socket_obj), err, port));
}

} // namespace

namespace Proud {

void SocketReportError(void *socket_obj, int err, void * /*msg_ctx*/) {
  if (!socket_obj || err == WSAEWOULDBLOCK || err == WSA_IO_PENDING)
    return;

  auto *base = reinterpret_cast<char *>(socket_obj);
  if (base[Proud::FastSocketLayout::kReportEnabled] != 1)
    return;

  // sub_D56170: formats a WString then calls IFastSocketDelegate::vt[1].
  // Offline PN FSM uses recv/connect return codes for state 5; the delegate
  // callback is not required to reach shard_choice on the happy path.
  log_socket_error(socket_obj, err);
}

} // namespace Proud
