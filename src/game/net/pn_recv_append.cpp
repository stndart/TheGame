#include "game/net/pn_recv_append.hpp"

#include "console.h"
#include "game/net/socket_trace.hpp"

namespace {

using EnsureSizeFn = void(__thiscall *)(void *self, int new_size);
using FmtErrorFn = void(__cdecl *)();

constexpr std::uintptr_t kEnsureSizeRva = 0xD71610;
constexpr std::uintptr_t kFmtErrorX2Rva = 0xCEFAE0;

constexpr std::uintptr_t kDataOff = 8;
constexpr std::uintptr_t kSizeOff = 12;

EnsureSizeFn g_orig_ensure_size =
    reinterpret_cast<EnsureSizeFn>(kEnsureSizeRva);
FmtErrorFn g_fmt_error_x2 = reinterpret_cast<FmtErrorFn>(kFmtErrorX2Rva);

void log_recv_chunk(const char *src, int len) {
  if (!src || len <= 0)
    return;
  const SOCKET sock = SocketTrace::socket_from_last_fast_ctx();
  if (sock == INVALID_SOCKET)
    return;
  const u_short port = SocketTrace::peer_port(sock);
  if (!SocketTrace::is_pn_track_port(port))
    return;
  logn(SocketTrace::net_log_key(sock), static_cast<size_t>(len),
       const_cast<char *>(src), true);
}

} // namespace

void PNRecvBuffer::append(const char *src, int len) {
  if (len < 0) {
    g_fmt_error_x2();
    return;
  }
  if (len == 0)
    return;

  auto *base = reinterpret_cast<char *>(this);
  const int cur_size =
      *reinterpret_cast<int *>(base + kSizeOff);

  g_orig_ensure_size(this, cur_size + len);

  char *dest = nullptr;
  if (*reinterpret_cast<int *>(base + kSizeOff) != 0)
    dest = *reinterpret_cast<char **>(base + kDataOff);

  for (int i = 0; i < len; ++i)
    dest[cur_size + i] = src[i];

  log_recv_chunk(src, len);
}
