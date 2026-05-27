#include "target_hooks.h"

#include "console.h"
#include "game/net/socket_trace.hpp"

// sub_D71CE0 — __thiscall append of completed WSARecv bytes (PN FSM state 3).

static void log_recv_chunk(void *self, const char *src, int len) {
  if (!src || len <= 0)
    return;
  const SOCKET sock = SocketTrace::read_fast_socket_slot(self);
  if (sock == INVALID_SOCKET)
    return;
  const u_short port = SocketTrace::peer_port(sock);
  if (port != SocketTrace::kEntryPort && port != SocketTrace::kGameLegPort)
    return;
  logn(SocketTrace::net_log_key(sock), static_cast<size_t>(len),
       const_cast<char *>(src), true);
}

extern "C" void __cdecl PnRecvBuffer_Append_Log(void *self, const char *src,
                                                int len) {
  log_recv_chunk(self, src, len);
}

extern "C" void __declspec(naked) hook_pn_recv_append() {
  __asm {
    push dword ptr [esp + 8]
    push dword ptr [esp + 8]
    push ecx
    call PnRecvBuffer_Append_Log
    add esp, 12

    push esi
    mov esi, [esp + 0Ch]
    push edi
    mov edi, ecx
    jmp dword ptr [g_target_pn_recv_append.return_addr]
  }
}

HookStub g_target_pn_recv_append = {
    0xD71CE0,
    (uint32_t)(uintptr_t)hook_pn_recv_append,
    "hook_pn_recv_append",
    {0},
    false,
    0xD71CE8,
};
