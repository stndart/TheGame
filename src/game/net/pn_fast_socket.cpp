#include "game/net/pn_fast_socket.hpp"

#include "console.h"
#include "game/net/pn_growable.hpp"
#include "game/net/pn_socket_error.hpp"
#include "game/net/socket_trace.hpp"
#include <cstring>
#include <winerror.h>
#include <winsock2.h>

namespace {

constexpr int kOverlapInFlightTag = 259;

// Proud::AddrPort @ CFastSocket+0xE4 - IPv4-mapped IPv6 + native-endian port.
struct PNAddrPort {
  std::uint8_t addr[16];
  std::uint16_t port;
};

// sub_CF0460 - refresh embedded AddrPort from connected peer after RX overlap.
std::uint16_t addr_port_update(PNAddrPort *addr_port, SOCKET sock) {
  if (!addr_port || sock == INVALID_SOCKET)
    return addr_port ? addr_port->port : 0;

  sockaddr_in peer{};
  int peer_len = static_cast<int>(sizeof(peer));
  if (getpeername(sock, reinterpret_cast<sockaddr *>(&peer), &peer_len) != 0)
    return addr_port->port;

  std::memset(addr_port->addr, 0, sizeof(addr_port->addr));
  addr_port->addr[10] = 0xff;
  addr_port->addr[11] = 0xff;
  std::memcpy(addr_port->addr + 12, &peer.sin_addr.s_addr, 4);
  addr_port->port = ntohs(peer.sin_port);
  return addr_port->port;
}

} // namespace

PNGrowableBuffer &PNFastSocket::send_staging() {
  return *reinterpret_cast<PNGrowableBuffer *>(reinterpret_cast<char *>(this) +
                                               pn::fast_socket::kStaging);
}

PNGrowableBuffer &PNFastSocket::recv_growable() {
  return *reinterpret_cast<PNGrowableBuffer *>(reinterpret_cast<char *>(this) +
                                               pn::fast_socket::kRecvGrowable);
}

char *PNFastSocket::send_staging_data() {
  auto &stg = send_staging();
  if (!stg.size_)
    return nullptr;
  return stg.data_;
}

const char *PNFastSocket::recv_staging_ptr() const {
  auto *base = reinterpret_cast<const char *>(this);
  if (!*reinterpret_cast<const int *>(base +
                                      pn::fast_socket::kRecvStagingActive))
    return nullptr;
  return *reinterpret_cast<const char *const *>(
      base + pn::fast_socket::kRecvStagingPtr);
}

void PNFastSocket::report_error(int err, void *msg_ctx) {
  pn::socket_report_error(this, err, msg_ctx);
}

char PNFastSocket::recv_complete(unsigned char wait, std::uint32_t *out) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  auto *overlap_tag =
      reinterpret_cast<DWORD *>(base + pn::fast_socket::kOverlapInFlight);
  if (*overlap_tag == kOverlapInFlightTag ||
      !base[pn::fast_socket::kRecvPending])
    return 0;

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + pn::fast_socket::kSocket);
  auto *overlapped =
      reinterpret_cast<LPWSAOVERLAPPED>(base + pn::fast_socket::kOverlapped);

  DWORD transferred = 0;
  DWORD flags = 0;
  const BOOL ok =
      WSAGetOverlappedResult(sock, overlapped, &transferred, wait, &flags);
  if (!ok && WSAGetLastError() == WSA_IO_PENDING)
    return 0;

  if (out) {
    out[0] = flags;
    if (!ok)
      out[1] = static_cast<std::uint32_t>(WSAGetLastError());
    else
      out[1] = 0;
    out[5] = transferred;
  }

  if (!ok)
    return 0;

  base[pn::fast_socket::kRecvPending] = 0;
  addr_port_update(
      reinterpret_cast<PNAddrPort *>(base + pn::fast_socket::kAddrPort), sock);

  logf("pn_recv_complete: fast=%p bytes=%lu", this,
       static_cast<unsigned long>(transferred));
  return 1;
}

int PNFastSocket::recv(int len) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  if (base[pn::fast_socket::kRecvIssueWarning]) {
    logf("pn_fast_socket: IssueRecv duplicated fast=%p", this);
  }

  if (len <= 0)
    return WSAEINVAL;

  pn::growable::ensure_size(recv_growable(), len);

  const char *staging = recv_staging_ptr();
  if (!staging)
    return WSAEINVAL;

  WSABUF wsa_buf{};
  wsa_buf.buf = const_cast<char *>(staging);
  wsa_buf.len = static_cast<ULONG>(len);

  DWORD bytes_recvd = 0;
  auto *flags = reinterpret_cast<DWORD *>(base + pn::fast_socket::kRecvFlags);
  *flags = 0;
  base[pn::fast_socket::kRecvPending] = 0;
  base[pn::fast_socket::kRecvPending] = 1;

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + pn::fast_socket::kSocket);
  auto *overlapped =
      reinterpret_cast<LPWSAOVERLAPPED>(base + pn::fast_socket::kOverlapped);

  int last_error = 0;
  while (true) {
    if (WSARecv(sock, &wsa_buf, 1, &bytes_recvd, flags, overlapped, nullptr) >=
        0)
      return 0;

    last_error = WSAGetLastError();
    if (last_error == WSA_IO_PENDING)
      return 0;
    if (last_error != WSAEINTR)
      break;

    pn::growable::note_wsa_interrupt_retry();
  }

  base[pn::fast_socket::kRecvPending] = 0;
  report_error(last_error, nullptr);
  return last_error;
}

char PNFastSocket::send_complete(unsigned char wait, std::uint32_t *out) {
  auto *base = reinterpret_cast<char *>(this);

  if (!base[pn::fast_socket::kSendPending])
    return 0;

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + pn::fast_socket::kSocket);
  auto *overlapped =
      reinterpret_cast<LPWSAOVERLAPPED>(base + pn::fast_socket::kSendOverlap);

  DWORD transferred = 0;
  DWORD flags = 0;
  if (!WSAGetOverlappedResult(sock, overlapped, &transferred, wait, &flags)) {
    const int err = WSAGetLastError();
    if (err == WSA_IO_PENDING)
      return 0;
    if (out) {
      out[0] = 0;
      out[1] = static_cast<std::uint32_t>(err);
      out[5] = 0;
    }
    return 0;
  }

  if (transferred == 0) {
    if (out) {
      out[0] = 0;
      out[1] = 0;
      out[5] = 0;
    }
    return 0;
  }

  if (out) {
    out[0] = 1;
    out[1] = 0;
    out[5] = transferred;
  }

  base[pn::fast_socket::kSendPending] = 0;
  return 1;
}

int PNFastSocket::send(void *data, int len) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  if (*reinterpret_cast<BYTE *>(base + pn::fast_socket::kSendWarning)) {
    logf("pn_fast_socket: IssueSend duplicated fast=%p", this);
  }

  if (len <= 0)
    return WSAEINVAL;

  pn::growable::ensure_size(send_staging(), len);

  char *staging = send_staging_data();
  if (staging)
    memcpy(staging, data, static_cast<size_t>(len));

  if (!staging)
    return WSAEINVAL;

  WSABUF buf{};
  buf.buf = staging;
  buf.len = static_cast<ULONG>(len);

  DWORD bytesSent = 0;
  auto *overlapped =
      reinterpret_cast<LPWSAOVERLAPPED>(base + pn::fast_socket::kOverlapped);
  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + pn::fast_socket::kSocket);

  *reinterpret_cast<BYTE *>(base + pn::fast_socket::kSendWarning) = 0;
  *reinterpret_cast<BYTE *>(base + pn::fast_socket::kSendWarning) = 1;

  int lastError = 0;
  while (true) {
    if (WSASend(sock, &buf, 1, &bytesSent, 0, overlapped, nullptr) >= 0) {
      *reinterpret_cast<BYTE *>(base + pn::fast_socket::kSendPending) = 1;
      ++*reinterpret_cast<int *>(base + pn::fast_socket::kSendOpCount);
      return 0;
    }

    lastError = WSAGetLastError();

    if (lastError == ERROR_IO_PENDING) {
      *reinterpret_cast<BYTE *>(base + pn::fast_socket::kSendPending) = 0;
      ++*reinterpret_cast<int *>(base + pn::fast_socket::kSendOpCount);
      return 0;
    }

    if (lastError != WSAEINTR)
      break;

    pn::growable::note_wsa_interrupt_retry();
  }

  *reinterpret_cast<BYTE *>(base + pn::fast_socket::kSendWarning) = 0;
  report_error(lastError, nullptr);
  return lastError;
}
