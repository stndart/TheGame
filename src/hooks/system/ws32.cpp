#include "hook_manager.h"
#include "system_hooks.h"
#include "game/net/pn_tcp_trace.hpp"
#include "game/net/socket_trace.hpp"
#include "game/server_override.hpp"

#include <console.h>
#include <winsock2.h>
#include <windows.h>

// Set to 1 to log every WS2_32 send/sendto/wsasend call site (very noisy).
#ifndef WS32_SYSLOGS
#define WS32_SYSLOGS 0
#endif

void __cdecl log_parg_n(int i, void *retaddr, void *p) {
#if WS32_SYSLOGS
  logf("Call[%i] from 0x%p with arg 0x%p", i, retaddr, p);
#else
  (void)i;
  (void)retaddr;
  (void)p;
#endif
}

extern "C" void __declspec(naked) send_syshandle() {
  __asm {
#if WS32_SYSLOGS
    pushad
    mov eax, [esp + 0x24]
    mov ebx, [esp + 0x20]
    push eax
    push ebx
    push 1
    call log_parg_n
    add esp, 12
    popad
#endif
    jmp g_ws2_send.sym_addr
  }
}

extern "C" void __declspec(naked) sendto_syshandle() {
  __asm {
#if WS32_SYSLOGS
    pushad
    mov eax, [esp + 0x24]
    mov ebx, [esp + 0x20]
    push eax
    push ebx
    push 2
    call log_parg_n
    add esp, 12
    popad
#endif
    jmp g_ws2_sendto.sym_addr
  }
}

using WSASendFn = int(WINAPI *)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD,
                                LPWSAOVERLAPPED,
                                LPWSAOVERLAPPED_COMPLETION_ROUTINE);

static void log_pn_wsasend_buffers(SOCKET s, LPWSABUF bufs, DWORD count) {
  if (!bufs || count == 0)
    return;
  const u_short peer = SocketTrace::pn_log_port(s);
  if (!SocketTrace::is_pn_track_port(peer) &&
      !SocketTrace::is_tracked_pn_socket(s))
    return;
  for (DWORD i = 0; i < count; ++i) {
    if (bufs[i].buf && bufs[i].len)
      PnTcpTrace::log_chunk(s, bufs[i].buf, bufs[i].len, false, nullptr);
  }
}

static int WSAAPI wsasend_forward(SOCKET s, LPWSABUF bufs, DWORD buffer_count,
                                  LPDWORD bytes_sent, DWORD flags,
                                  LPWSAOVERLAPPED overlapped,
                                  LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  log_pn_wsasend_buffers(s, bufs, buffer_count);
  return reinterpret_cast<WSASendFn>(g_ws2_wsasend.sym_addr)(
      s, bufs, buffer_count, bytes_sent, flags, overlapped, completion);
}

extern "C" int WSAAPI wsasend_syshandle(SOCKET s, LPWSABUF bufs,
                                        DWORD buffer_count, LPDWORD bytes_sent,
                                        DWORD flags, LPWSAOVERLAPPED overlapped,
                                        LPWSAOVERLAPPED_COMPLETION_ROUTINE
                                            completion) {
  return wsasend_forward(s, bufs, buffer_count, bytes_sent, flags, overlapped,
                         completion);
}

extern "C" void __declspec(naked) wsasendto_syshandle() {
  __asm {
#if WS32_SYSLOGS
    pushad
    mov eax, [esp + 0x24]
    mov ebx, [esp + 0x20]
    push eax
    push ebx
    push 4
    call log_parg_n
    add esp, 12
    popad
#endif
    jmp g_ws2_wsasendto.sym_addr
  }
}

using ConnectFn = int(WINAPI *)(SOCKET, const sockaddr *, int);

static int WSAAPI connect_forward(SOCKET s, const sockaddr *name, int namelen) {
  return reinterpret_cast<ConnectFn>(g_ws2_connect.sym_addr)(s, name, namelen);
}

static int WSAAPI connect_with_remap(SOCKET s, const sockaddr *name, int namelen) {
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
        logf("connect:27380 %s -> %s", inet_ntoa(was), inet_ntoa(patched.sin_addr));
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

SysHookStub g_ws2_send = {"ws2_32.dll", "send", send_syshandle};
SysHookStub g_ws2_sendto = {"ws2_32.dll", "sendto", sendto_syshandle};
SysHookStub g_ws2_wsasend = {
    "ws2_32.dll", "WSASend",
    reinterpret_cast<void (*)()>(wsasend_syshandle)};
SysHookStub g_ws2_wsasendto = {"ws2_32.dll", "WSASendTo", wsasendto_syshandle};
SysHookStub g_ws2_connect = {
    "ws2_32.dll", "connect",
    reinterpret_cast<void (*)()>(connect_syshandle)};
