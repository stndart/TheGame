#include "target_hooks.h"

#include "console.h"
#include "game/net/socket_trace.hpp"
#include "game/server_override.hpp"

// Proud::CFastSocket — PNCliWorker w_wsasend_1 / w_wsarecv_1 (not TCPSocket::Send).
// Disabled in main.cpp: enabling hook_fast_wsasend alongside hook_send_2 crashes
// (same RVA 0xD567F0). Kept for future RE / alternate hook strategy.

using FastSocketSendFn = int(__thiscall *)(void *self, void *data, int len);
using FastSocketRecvFn = int(__thiscall *)(void *self, int len);

static FastSocketSendFn g_orig_fast_send =
    reinterpret_cast<FastSocketSendFn>(0xD567F7);
static FastSocketRecvFn g_orig_fast_recv =
    reinterpret_cast<FastSocketRecvFn>(0xD56477);

static SOCKET socket_from_fast(void *self) {
  if (!self)
    return INVALID_SOCKET;
  return *reinterpret_cast<SOCKET *>(static_cast<char *>(self) + 0x12C);
}

static bool should_hex_log(SOCKET sock) {
  const u_short port = SocketTrace::peer_port(sock);
  return port == SocketTrace::kEntryPort || port == SocketTrace::kGameLegPort;
}

int __fastcall ProudFastSocket_Send(void *self, void * /*edx*/, void *data,
                                    int len) {
  const SOCKET sock = socket_from_fast(self);
  if (should_hex_log(sock) && data && len > 0)
    logn(SocketTrace::net_log_key(sock), static_cast<size_t>(len),
         static_cast<char *>(data), false);
  return g_orig_fast_send(self, data, len);
}

int __fastcall ProudFastSocket_Recv(void *self, void * /*edx*/, int len) {
  return g_orig_fast_recv(self, len);
}

extern "C" void __declspec(naked) hook_fast_wsasend() {
  __asm { jmp ProudFastSocket_Send }
}

extern "C" void __declspec(naked) hook_fast_wsarecv() {
  __asm { jmp ProudFastSocket_Recv }
}

HookStub g_target_fast_wsasend = {0xD567F0,
                                  (uint32_t)(uintptr_t)hook_fast_wsasend,
                                  "hook_fast_wsasend",
                                  {0},
                                  false,
                                  0xD567F7};

HookStub g_target_fast_wsarecv = {0xD56470,
                                  (uint32_t)(uintptr_t)hook_fast_wsarecv,
                                  "hook_fast_wsarecv",
                                  {0},
                                  false,
                                  0xD56477};
