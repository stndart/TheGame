#include "game/net/pn_fast_send.hpp"

#include "console.h"
#include "game/engine/TCPSocket.h"
#include "game/engine/WString.h"
#include "game/net/socket_trace.hpp"

#include <cstring>
#include <winerror.h>
#include <winsock2.h>

namespace {

using EnsureStagingFn = void(__thiscall *)(void *staging, int new_size);
using ReportSocketErrorFn = int(__thiscall *)(void *self, int err, void *ctx);

constexpr std::uintptr_t kEnsureStagingRva = 0x9FC720;
constexpr std::uintptr_t kReportSocketErrorRva = 0xD56170;
constexpr std::uintptr_t kInterruptCounterRva = 0x180DF00;
constexpr std::uintptr_t kSendErrorCtxRva = 0x15DC28C;

constexpr std::uintptr_t kCallbackOff = 16;
constexpr std::uintptr_t kStagingOff = 0xC0;
constexpr std::uintptr_t kStagingDataOff = 8;
constexpr std::uintptr_t kStagingFlagOff = 12;
constexpr std::uintptr_t kOverlappedOff = 168;
constexpr std::uintptr_t kSendWarningOff = 0xBC;
constexpr std::uintptr_t kSendPendingOff = 0x12A;
constexpr std::uintptr_t kSendOpCountOff = 0x104;
constexpr std::uintptr_t kSocketOff = 0x12C;

EnsureStagingFn g_ensure_staging =
    reinterpret_cast<EnsureStagingFn>(kEnsureStagingRva);
ReportSocketErrorFn g_report_socket_error =
    reinterpret_cast<ReportSocketErrorFn>(kReportSocketErrorRva);

char *staging_data_ptr(char *base) {
  auto *staging = base + kStagingOff;
  if (!*reinterpret_cast<int *>(staging + kStagingFlagOff))
    return nullptr;
  return *reinterpret_cast<char **>(staging + kStagingDataOff);
}

void log_tx_if_tracked(char *base, const void *data, int len) {
  if (!data || len <= 0)
    return;
  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + kSocketOff);
  if (!SocketTrace::is_pn_track_port(SocketTrace::peer_port(sock)))
    return;
  logn(SocketTrace::net_log_key(sock), static_cast<size_t>(len),
       const_cast<char *>(static_cast<const char *>(data)), false);
}

} // namespace

int PNFastSocket::send(void *data, int len) {
  auto *base = reinterpret_cast<char *>(this);
  SocketTrace::note_fast_socket_ctx(this);

  if (*reinterpret_cast<BYTE *>(base + kSendWarningOff)) {
    WString warningMsg = L"WARNING: IssueSend is duplicated!";
    auto *cb = *reinterpret_cast<NetCallbackInterface **>(base + kCallbackOff);
    if (cb)
      cb->OnWarning(reinterpret_cast<TCPSocket *>(this), &warningMsg);
  }

  if (len <= 0)
    return WSAEINVAL;

  g_ensure_staging(base + kStagingOff, len);

  char *staging = staging_data_ptr(base);
  if (staging)
    memcpy(staging, data, static_cast<size_t>(len));

  if (!staging)
    return WSAEINVAL;

  log_tx_if_tracked(base, data, len);

  WSABUF buf{};
  buf.buf = staging;
  buf.len = static_cast<ULONG>(len);

  DWORD bytesSent = 0;
  auto *overlapped =
      reinterpret_cast<LPWSAOVERLAPPED>(base + kOverlappedOff);
  const SOCKET sock = *reinterpret_cast<SOCKET *>(base + kSocketOff);

  *reinterpret_cast<BYTE *>(base + kSendWarningOff) = 0;
  *reinterpret_cast<BYTE *>(base + kSendWarningOff) = 1;

  int lastError = 0;
  while (true) {
    if (WSASend(sock, &buf, 1, &bytesSent, 0, overlapped, nullptr) >= 0) {
      *reinterpret_cast<BYTE *>(base + kSendPendingOff) = 1;
      ++*reinterpret_cast<int *>(base + kSendOpCountOff);
      return 0;
    }

    lastError = WSAGetLastError();

    if (lastError == ERROR_IO_PENDING) {
      *reinterpret_cast<BYTE *>(base + kSendPendingOff) = 0;
      ++*reinterpret_cast<int *>(base + kSendOpCountOff);
      return 0;
    }

    if (lastError != WSAEINTR)
      break;

    InterlockedIncrement(reinterpret_cast<volatile LONG *>(kInterruptCounterRva));
  }

  *reinterpret_cast<BYTE *>(base + kSendWarningOff) = 0;
  g_report_socket_error(this, lastError,
                        reinterpret_cast<void *>(kSendErrorCtxRva));
  return lastError;
}
