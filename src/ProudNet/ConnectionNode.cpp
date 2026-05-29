#include "ProudNet/ConnectionNode.hpp"

#include "console.h"
#include "game/engine/String.h"
#include "game/engine/TCPSocket.h"
#include "game/engine/WString.h"
#include "ProudNet/FastSocket.hpp"
#include "ProudNet/SelectContext.hpp"
#include "ProudNet/SelectFd.hpp"
#include "ProudNet/UpnpClient.hpp"

#include <winsock2.h>

namespace {

constexpr std::uintptr_t kGatewayHostWide = 90;
constexpr std::uintptr_t kGatewayPort = 9316;

void set_fast_socket_nonblock(Proud::CFastSocket *fast) {
  if (!fast)
    return;
  auto *base = reinterpret_cast<char *>(fast);
  SOCKET s = *reinterpret_cast<SOCKET *>(base + Proud::FastSocketLayout::kSocket);
  u_long mode = 1;
  ioctlsocket(s, FIONBIO, &mode);
}

// sub_D6EF90 poll_out != 0: except fd set - retry TCPSocket::Connect from
// gateway.
char retry_connect_on_poll_except(Proud::ConnectionNode *conn) {
  auto *base = reinterpret_cast<char *>(conn);
  auto *gateway = *reinterpret_cast<char **>(base + Proud::conn::kGatewayCtx);
  if (!gateway) {
    conn->fsm_state = 5;
    return 0;
  }

  const auto port = *reinterpret_cast<std::uint16_t *>(gateway + kGatewayPort);
  const wchar_t *host_wide =
      reinterpret_cast<const wchar_t *>(gateway + kGatewayHostWide);

  Proud::CFastSocket *const sock = conn->fast();
  if (!sock) {
    conn->fsm_state = 5;
    return 0;
  }

  const size_t host_len = wcslen(host_wide);
  WString hostname(host_wide, host_len);
  const int err = reinterpret_cast<TCPSocket *>(sock)->Connect(hostname, port);
  if (err == 0 || err == WSAEWOULDBLOCK)
    return 1;

  conn->fsm_state = 5;
  return 0;
}

} // namespace

Proud::SendArm &Proud::ConnectionNode::send_arm() {
  return *reinterpret_cast<Proud::SendArm *>(reinterpret_cast<char *>(this) +
                                        Proud::conn::kSendArm);
}

Proud::CFastSocket *Proud::ConnectionNode::fast() const {
  if (!fast_socket)
    return nullptr;
  return *fast_socket;
}

int Proud::ConnectionNode::outbound_remaining() const {
  const auto *self = this;
  const auto &arm = const_cast<Proud::ConnectionNode *>(self)->send_arm();
  const int total = static_cast<int>(arm.outbound.GetLength());
  return total - arm.send_offset;
}

bool Proud::SendArm::arm_send() {
  const int total = static_cast<int>(outbound.GetLength());
  const int offset = send_offset;
  const int slice = total - offset;
  if (slice <= 0)
    return false;

  Proud::CFastSocket *sock = nullptr;
  if (fast_socket)
    sock = *fast_socket;
  if (!sock)
    return false;

  send_pending = 1;
  const char *data = outbound.c_str();
  if (sock->send(const_cast<char *>(data + offset), slice) != 0) {
    send_pending = 0;
    return false;
  }
  return true;
}

char Proud::ConnectionNode::run_state_1() {
  logf("pn_conn: state1 conn=%p fast=%p", this, fast());

  alignas(4) std::uint8_t select_buf[0x220];
  auto *sel = reinterpret_cast<Proud::CSelectContext *>(select_buf);
  Proud::select_fd::init(sel);

  Proud::CFastSocket *const sock = fast();
  if (sock) {
    Proud::select_fd::add_read(sel, sock);
    Proud::select_fd::add_except(sel, sock);
  }

  Proud::select_fd::wait(sel, 0);

  std::uint32_t poll_out = 0;
  const char ready = sock ? sel->poll(sock, &poll_out) : 0;
  if (!ready)
    return 0;

  if (poll_out != 0)
    return retry_connect_on_poll_except(this);

  set_fast_socket_nonblock(sock);

  auto *base = reinterpret_cast<char *>(this);
  const int upnp_mode = *reinterpret_cast<int *>(base + Proud::conn::kUpnpSubmode);
  auto *upnp = reinterpret_cast<Proud::UpnpClient *>(this);
  if (upnp_mode == 1) {
    upnp->DeletePortMapping();
  } else if (upnp_mode == 0) {
    upnp->AddPortMapping();
  } else {
    fsm_state = 5;
    return 0;
  }

  const bool armed = send_arm().arm_send();
  fsm_state = armed ? 2 : 5;
  return armed ? 1 : 0;
}

char Proud::ConnectionNode::run_state_2() {
  auto *base = reinterpret_cast<char *>(this);
  std::uint32_t overlap_out[8] = {};

  logf("pn_conn: state2 conn=%p fast=%p", this, fast());

  Proud::CFastSocket *const sock = fast();
  if (!sock)
    return 0;

  if (!sock->send_complete(0, overlap_out))
    return 0;

  base[Proud::conn::kSendPendingConn] = 0;

  const int bytes = static_cast<int>(overlap_out[5]);
  if (bytes < 0) {
    fsm_state = 5;
    return 0;
  }

  send_arm().send_offset += bytes;
  *reinterpret_cast<int *>(base + Proud::conn::kSendBytesDone) += bytes;

  if (outbound_remaining() > 0) {
    if (send_arm().arm_send())
      return 1;
    fsm_state = 5;
    return 0;
  }

  base[Proud::conn::kRecvPending] = 1;
  if (sock->recv(0x8000) == 0) {
    fsm_state = 3;
    return 0;
  }

  base[Proud::conn::kRecvPending] = 0;
  fsm_state = 5;
  return 0;
}

int Proud::ConnectionNode::run_state_3() {
  auto *base = reinterpret_cast<char *>(this);
  std::uint32_t overlap_out[8] = {};

  logf("pn_conn: state3 conn=%p fast=%p recv.size=%d", this, fast(),
       recv.size_);

  Proud::CFastSocket *const sock = fast();
  if (!sock)
    return 0;

  if (!sock->recv_complete(0, overlap_out))
    return 0;

  const int chunk = static_cast<int>(overlap_out[5]);
  base[Proud::conn::kRecvPending] = 0;

  if (chunk <= 0) {
    fsm_state = 4;
    return 0;
  }

  const char *src = sock->recv_staging_ptr();
  if (src)
    recv.append(src, chunk);

  *reinterpret_cast<int *>(base + Proud::conn::kRecvBytesTotal) += chunk;

  const int remaining =
      0x8000 - *reinterpret_cast<int *>(base + Proud::conn::kRecvBytesTotal);
  if (remaining <= 0) {
    fsm_state = 4;
    return 0;
  }

  base[Proud::conn::kRecvPending] = 1;
  if (sock->recv(remaining) != 0) {
    fsm_state = 5;
    return 0;
  }

  return 0;
}
