#include "game/net/pn_socket_error.hpp"

#include "console.h"
#include "game/net/pn_layout.hpp"
#include "game/net/socket_trace.hpp"

#include <winsock2.h>

namespace {

void log_socket_error(void *socket_obj, int err) {
  auto *base = reinterpret_cast<char *>(socket_obj);
  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + pn::fast_socket::kSocket);
  const u_short port = SocketTrace::peer_port(sock);
  logf("pn_socket_error: fast=%p sock=%p err=%d port=%u", socket_obj,
       reinterpret_cast<void *>(sock), err, static_cast<unsigned>(port));
}

} // namespace

namespace pn {

void socket_report_error(void *socket_obj, int err, void * /*msg_ctx*/) {
  if (!socket_obj || err == WSAEWOULDBLOCK || err == WSA_IO_PENDING)
    return;

  auto *base = reinterpret_cast<char *>(socket_obj);
  if (base[pn::fast_socket::kReportEnabled] != 1)
    return;

  // sub_D56170: formats a WString then calls IFastSocketDelegate::vt[1].
  // Offline PN FSM uses recv/connect return codes for state 5; the delegate
  // callback is not required to reach shard_choice on the happy path.
  log_socket_error(socket_obj, err);
}

} // namespace pn
