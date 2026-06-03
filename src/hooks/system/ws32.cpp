#include "system_hooks.h"

#include <windows.h>
#include <winsock2.h>

#include "ProudNet/TcpTrace.hpp"
#include "game/net/socket_trace.hpp"
#include "game/server_override.hpp"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

using thegame::LogMessage;
using thegame::logn;

namespace {

void log_tcp_payload(SOCKET s, const void *data, size_t len, bool inbound) {
  if (thegame::cfg.no_network_logs || !data || len == 0)
    return;
  logn(static_cast<int>(s), len,
       const_cast<char *>(static_cast<const char *>(data)), inbound);
}

void log_udp_len(SOCKET s, size_t len, bool inbound) {
  if (thegame::cfg.no_network_logs || len == 0)
    return;
  logn(static_cast<int>(s),
       LogMessage("{} UDP, payload len {}", inbound ? "rx" : "tx", len));
}

void log_pn_wsasend_buffers(SOCKET s, LPWSABUF bufs, DWORD count) {
  if (!bufs || count == 0)
    return;

  const u_short peer = SocketTrace::pn_log_port(s);
  const bool pn_trace = SocketTrace::is_pn_track_port(peer) ||
                        SocketTrace::is_tracked_pn_socket(s);

  // TX: higher layer (proud) before lower layer (net).
  if (pn_trace) {
    for (DWORD i = 0; i < count; ++i) {
      if (bufs[i].buf && bufs[i].len)
        Proud::TcpTrace::log_chunk(s, bufs[i].buf, bufs[i].len, false, nullptr);
    }
  }

  for (DWORD i = 0; i < count; ++i) {
    if (bufs[i].buf && bufs[i].len)
      log_tcp_payload(s, bufs[i].buf, bufs[i].len, false);
  }
}

void log_pn_wsarecv_buffers(SOCKET s, LPWSABUF bufs, DWORD count, DWORD bytes) {
  if (!bufs || count == 0 || bytes == 0)
    return;

  const u_short peer = SocketTrace::pn_log_port(s);
  const bool pn_trace = SocketTrace::is_pn_track_port(peer) ||
                        SocketTrace::is_tracked_pn_socket(s);

  // RX: lower layer (net) before higher layer (proud).
  DWORD remaining = bytes;
  for (DWORD i = 0; i < count && remaining > 0; ++i) {
    if (!bufs[i].buf || bufs[i].len == 0)
      continue;
    const DWORD chunk = remaining < bufs[i].len ? remaining : bufs[i].len;
    log_tcp_payload(s, bufs[i].buf, chunk, true);
    remaining -= chunk;
  }

  if (!pn_trace)
    return;

  remaining = bytes;
  for (DWORD i = 0; i < count && remaining > 0; ++i) {
    if (!bufs[i].buf || bufs[i].len == 0)
      continue;
    const DWORD chunk = remaining < bufs[i].len ? remaining : bufs[i].len;
    Proud::TcpTrace::log_chunk(s, bufs[i].buf, chunk, true, nullptr);
    remaining -= chunk;
  }
}

DWORD wsabuf_total_len(LPWSABUF bufs, DWORD count) {
  if (!bufs)
    return 0;
  DWORD total = 0;
  for (DWORD i = 0; i < count; ++i)
    total += bufs[i].len;
  return total;
}

} // namespace

using SendFn = int(WINAPI *)(SOCKET, const char *, int, int);
using SendToFn = int(WINAPI *)(SOCKET, const char *, int, int, const sockaddr *,
                               int);
using RecvFn = int(WINAPI *)(SOCKET, char *, int, int);
using RecvFromFn = int(WINAPI *)(SOCKET, char *, int, int, sockaddr *, int *);
using WSASendFn = int(WINAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD,
                                LPWSAOVERLAPPED,
                                LPWSAOVERLAPPED_COMPLETION_ROUTINE);
using WSASendToFn = int(WINAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD,
                                  const sockaddr *, int, LPWSAOVERLAPPED,
                                  LPWSAOVERLAPPED_COMPLETION_ROUTINE);
using WSARecvFn = int(WINAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD,
                                LPWSAOVERLAPPED,
                                LPWSAOVERLAPPED_COMPLETION_ROUTINE);
using WSARecvFromFn = int(WINAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, LPDWORD,
                                    sockaddr *, LPINT, LPWSAOVERLAPPED,
                                    LPWSAOVERLAPPED_COMPLETION_ROUTINE);
using ConnectFn = int(WINAPI *)(SOCKET, const sockaddr *, int);

extern "C" int WSAAPI send_syshandle(SOCKET s, const char *buf, int len,
                                     int flags) {
  log_tcp_payload(s, buf, static_cast<size_t>(len), false);
  return reinterpret_cast<SendFn>(g_ws2_send.sym_addr)(s, buf, len, flags);
}

extern "C" int WSAAPI sendto_syshandle(SOCKET s, const char *buf, int len,
                                       int flags, const sockaddr *to,
                                       int tolen) {
  log_udp_len(s, static_cast<size_t>(len), false);
  return reinterpret_cast<SendToFn>(g_ws2_sendto.sym_addr)(s, buf, len, flags,
                                                             to, tolen);
}

static int WSAAPI
wsasend_forward(SOCKET s, LPWSABUF bufs, DWORD buffer_count, LPDWORD bytes_sent,
                DWORD flags, LPWSAOVERLAPPED overlapped,
                LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  log_pn_wsasend_buffers(s, bufs, buffer_count);
  return reinterpret_cast<WSASendFn>(g_ws2_wsasend.sym_addr)(
      s, bufs, buffer_count, bytes_sent, flags, overlapped, completion);
}

extern "C" int WSAAPI
wsasend_syshandle(SOCKET s, LPWSABUF bufs, DWORD buffer_count,
                  LPDWORD bytes_sent, DWORD flags, LPWSAOVERLAPPED overlapped,
                  LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  return wsasend_forward(s, bufs, buffer_count, bytes_sent, flags, overlapped,
                         completion);
}

extern "C" int WSAAPI
wsasendto_syshandle(SOCKET s, LPWSABUF bufs, DWORD buffer_count,
                    LPDWORD bytes_sent, DWORD flags, const sockaddr *to,
                    int tolen, LPWSAOVERLAPPED overlapped,
                    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  log_udp_len(s, wsabuf_total_len(bufs, buffer_count), false);
  return reinterpret_cast<WSASendToFn>(g_ws2_wsasendto.sym_addr)(
      s, bufs, buffer_count, bytes_sent, flags, to, tolen, overlapped,
      completion);
}

extern "C" int WSAAPI recv_syshandle(SOCKET s, char *buf, int len, int flags) {
  const int rc =
      reinterpret_cast<RecvFn>(g_ws2_recv.sym_addr)(s, buf, len, flags);
  if (rc > 0)
    log_tcp_payload(s, buf, static_cast<size_t>(rc), true);
  return rc;
}

extern "C" int WSAAPI recvfrom_syshandle(SOCKET s, char *buf, int len,
                                         int flags, sockaddr *from,
                                         int *fromlen) {
  const int rc = reinterpret_cast<RecvFromFn>(g_ws2_recvfrom.sym_addr)(
      s, buf, len, flags, from, fromlen);
  if (rc > 0)
    log_udp_len(s, static_cast<size_t>(rc), true);
  return rc;
}

static int WSAAPI
wsarecv_forward(SOCKET s, LPWSABUF bufs, DWORD buffer_count, LPDWORD bytes_recvd,
                LPDWORD flags, LPWSAOVERLAPPED overlapped,
                LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  const int rc = reinterpret_cast<WSARecvFn>(g_ws2_wsarecv.sym_addr)(
      s, bufs, buffer_count, bytes_recvd, flags, overlapped, completion);
  if (rc == 0 && bytes_recvd && *bytes_recvd > 0)
    log_pn_wsarecv_buffers(s, bufs, buffer_count, *bytes_recvd);
  return rc;
}

extern "C" int WSAAPI
wsarecv_syshandle(SOCKET s, LPWSABUF bufs, DWORD buffer_count,
                  LPDWORD bytes_recvd, LPDWORD flags,
                  LPWSAOVERLAPPED overlapped,
                  LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  return wsarecv_forward(s, bufs, buffer_count, bytes_recvd, flags, overlapped,
                         completion);
}

extern "C" int WSAAPI wsarecvfrom_syshandle(
    SOCKET s, LPWSABUF bufs, DWORD buffer_count, LPDWORD bytes_recvd,
    LPDWORD flags, sockaddr *from, LPINT fromlen, LPWSAOVERLAPPED overlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  const int rc = reinterpret_cast<WSARecvFromFn>(g_ws2_wsarecvfrom.sym_addr)(
      s, bufs, buffer_count, bytes_recvd, flags, from, fromlen, overlapped,
      completion);
  if (rc == 0 && bytes_recvd && *bytes_recvd > 0)
    log_udp_len(s, *bytes_recvd, true);
  return rc;
}

static int WSAAPI connect_forward(SOCKET s, const sockaddr *name, int namelen) {
  return reinterpret_cast<ConnectFn>(g_ws2_connect.sym_addr)(s, name, namelen);
}

static int WSAAPI connect_with_remap(SOCKET s, const sockaddr *name,
                                     int namelen) {
  sockaddr_in patched{};
  const sockaddr *peer = name;
  if (name && namelen >= static_cast<int>(sizeof(sockaddr_in)) &&
      name->sa_family == AF_INET) {
    const auto *in = reinterpret_cast<const sockaddr_in *>(name);
    patched = *in;
    if (ServerOverride::remap_sockaddr_in(&patched)) {
      peer = reinterpret_cast<const sockaddr *>(&patched);
      const u_short port = ntohs(patched.sin_port);
      if (port == ServerOverride::kGameLegPort) {
        in_addr was{};
        was.S_un.S_addr = in->sin_addr.s_addr;
        thegame::logf(thegame::LogMessage(
            thegame::LogSource::Net,
            fmt::format("connect:27380 {} -> {}", inet_ntoa(was),
                        inet_ntoa(patched.sin_addr))));
      }
    }
  }
  const int rc = connect_forward(s, peer, namelen);
  if (name && namelen >= static_cast<int>(sizeof(sockaddr_in)) &&
      name->sa_family == AF_INET) {
    const auto *in = reinterpret_cast<const sockaddr_in *>(peer);
    const u_short port = ntohs(in->sin_port);
    if (SocketTrace::is_pn_track_port(port) &&
        (rc == 0 || WSAGetLastError() == WSAEWOULDBLOCK))
      SocketTrace::track_connect(s, port);
  }
  return rc;
}

extern "C" int WSAAPI connect_syshandle(SOCKET s, const sockaddr *name,
                                        int namelen) {
  return connect_with_remap(s, name, namelen);
}

SysHookStub g_ws2_send = {"ws2_32.dll", "send",
                          reinterpret_cast<void (*)()>(send_syshandle)};
SysHookStub g_ws2_sendto = {"ws2_32.dll", "sendto",
                            reinterpret_cast<void (*)()>(sendto_syshandle)};
SysHookStub g_ws2_recv = {"ws2_32.dll", "recv",
                          reinterpret_cast<void (*)()>(recv_syshandle)};
SysHookStub g_ws2_recvfrom = {"ws2_32.dll", "recvfrom",
                              reinterpret_cast<void (*)()>(recvfrom_syshandle)};
SysHookStub g_ws2_wsasend = {"ws2_32.dll", "WSASend",
                             reinterpret_cast<void (*)()>(wsasend_syshandle)};
SysHookStub g_ws2_wsasendto = {"ws2_32.dll", "WSASendTo",
                               reinterpret_cast<void (*)()>(wsasendto_syshandle)};
SysHookStub g_ws2_wsarecv = {"ws2_32.dll", "WSARecv",
                             reinterpret_cast<void (*)()>(wsarecv_syshandle)};
SysHookStub g_ws2_wsarecvfrom = {
    "ws2_32.dll", "WSARecvFrom",
    reinterpret_cast<void (*)()>(wsarecvfrom_syshandle)};
SysHookStub g_ws2_connect = {"ws2_32.dll", "connect",
                             reinterpret_cast<void (*)()>(connect_syshandle)};
