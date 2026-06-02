#include "ProudNet/FastSocket.hpp"

#include <cstring>
#include <winerror.h>
#include <winsock2.h>

#include "ProudNet/GrowableBuffer.hpp"
#include "ProudNet/SocketError.hpp"
#include "game/net/socket_trace.hpp"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

using thegame::logpnf;

namespace {

constexpr int kOverlapInFlightTag = 259;

// Proud::AddrPort @ Proud::CFastSocket+0xE4 - IPv4-mapped IPv6 + native-endian
// port.
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

Proud::GrowableBuffer &Proud::CFastSocket::send_staging() {
  return *reinterpret_cast<Proud::GrowableBuffer *>(
      reinterpret_cast<char *>(this) + Proud::FastSocketLayout::kStaging);
}

Proud::GrowableBuffer &Proud::CFastSocket::recv_growable() {
  return *reinterpret_cast<Proud::GrowableBuffer *>(
      reinterpret_cast<char *>(this) + Proud::FastSocketLayout::kRecvGrowable);
}

char *Proud::CFastSocket::send_staging_data() {
  auto &stg = send_staging();
  if (!stg.size_)
    return nullptr;
  return stg.data_;
}

const char *Proud::CFastSocket::recv_staging_ptr() const {
  auto *base = reinterpret_cast<const char *>(this);
  if (!*reinterpret_cast<const int *>(
          base + Proud::FastSocketLayout::kRecvStagingActive))
    return nullptr;
  return *reinterpret_cast<const char *const *>(
      base + Proud::FastSocketLayout::kRecvStagingPtr);
}

void Proud::CFastSocket::report_error(int err, void *msg_ctx) {
  Proud::SocketReportError(this, err, msg_ctx);
}

char Proud::CFastSocket::recv_complete(unsigned char wait, std::uint32_t *out) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  auto *overlap_tag = reinterpret_cast<DWORD *>(
      base + Proud::FastSocketLayout::kOverlapInFlight);
  if (*overlap_tag == kOverlapInFlightTag ||
      !base[Proud::FastSocketLayout::kRecvPending])
    return 0;

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + Proud::FastSocketLayout::kSocket);
  auto *overlapped = reinterpret_cast<LPWSAOVERLAPPED>(
      base + Proud::FastSocketLayout::kOverlapped);

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

  base[Proud::FastSocketLayout::kRecvPending] = 0;
  addr_port_update(
      reinterpret_cast<PNAddrPort *>(base + Proud::FastSocketLayout::kAddrPort),
      sock);

  if (!thegame::cfg.no_proud_logs)
    logpnf(static_cast<int>(sock), "recv_complete fast=%p bytes=%lu", this,
           static_cast<unsigned long>(transferred));
  return 1;
}

int Proud::CFastSocket::recv(int len) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + Proud::FastSocketLayout::kSocket);

  if (base[Proud::FastSocketLayout::kRecvIssueWarning] &&
      !thegame::cfg.no_proud_logs) {
    logpnf(static_cast<int>(sock), "IssueRecv duplicated fast=%p", this);
  }

  if (len <= 0)
    return WSAEINVAL;

  Proud::growable::ensure_size(recv_growable(), len);

  const char *staging = recv_staging_ptr();
  if (!staging)
    return WSAEINVAL;

  WSABUF wsa_buf{};
  wsa_buf.buf = const_cast<char *>(staging);
  wsa_buf.len = static_cast<ULONG>(len);

  DWORD bytes_recvd = 0;
  auto *flags =
      reinterpret_cast<DWORD *>(base + Proud::FastSocketLayout::kRecvFlags);
  *flags = 0;
  base[Proud::FastSocketLayout::kRecvPending] = 0;
  base[Proud::FastSocketLayout::kRecvPending] = 1;

  auto *overlapped = reinterpret_cast<LPWSAOVERLAPPED>(
      base + Proud::FastSocketLayout::kOverlapped);

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

    Proud::growable::note_wsa_interrupt_retry();
  }

  base[Proud::FastSocketLayout::kRecvPending] = 0;
  report_error(last_error, nullptr);
  return last_error;
}

char Proud::CFastSocket::send_complete(unsigned char wait, std::uint32_t *out) {
  auto *base = reinterpret_cast<char *>(this);

  if (!base[Proud::FastSocketLayout::kSendPending])
    return 0;

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + Proud::FastSocketLayout::kSocket);
  auto *overlapped = reinterpret_cast<LPWSAOVERLAPPED>(
      base + Proud::FastSocketLayout::kSendOverlap);

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

  base[Proud::FastSocketLayout::kSendPending] = 0;
  return 1;
}

int Proud::CFastSocket::send(void *data, int len) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + Proud::FastSocketLayout::kSocket);

  if (*reinterpret_cast<BYTE *>(base + Proud::FastSocketLayout::kSendWarning) &&
      !thegame::cfg.no_proud_logs) {
    logpnf(static_cast<int>(sock), "IssueSend duplicated fast=%p", this);
  }

  if (len <= 0)
    return WSAEINVAL;

  Proud::growable::ensure_size(send_staging(), len);

  char *staging = send_staging_data();
  if (staging)
    memcpy(staging, data, static_cast<size_t>(len));

  if (!staging)
    return WSAEINVAL;

  WSABUF buf{};
  buf.buf = staging;
  buf.len = static_cast<ULONG>(len);

  DWORD bytesSent = 0;
  auto *overlapped = reinterpret_cast<LPWSAOVERLAPPED>(
      base + Proud::FastSocketLayout::kOverlapped);

  *reinterpret_cast<BYTE *>(base + Proud::FastSocketLayout::kSendWarning) = 0;
  *reinterpret_cast<BYTE *>(base + Proud::FastSocketLayout::kSendWarning) = 1;

  int lastError = 0;
  while (true) {
    if (WSASend(sock, &buf, 1, &bytesSent, 0, overlapped, nullptr) >= 0) {
      *reinterpret_cast<BYTE *>(base + Proud::FastSocketLayout::kSendPending) =
          1;
      ++*reinterpret_cast<int *>(base + Proud::FastSocketLayout::kSendOpCount);
      return 0;
    }

    lastError = WSAGetLastError();

    if (lastError == ERROR_IO_PENDING) {
      *reinterpret_cast<BYTE *>(base + Proud::FastSocketLayout::kSendPending) =
          0;
      ++*reinterpret_cast<int *>(base + Proud::FastSocketLayout::kSendOpCount);
      return 0;
    }

    if (lastError != WSAEINTR)
      break;

    Proud::growable::note_wsa_interrupt_retry();
  }

  *reinterpret_cast<BYTE *>(base + Proud::FastSocketLayout::kSendWarning) = 0;
  report_error(lastError, nullptr);
  return lastError;
}
