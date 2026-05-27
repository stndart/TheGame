#include "target_hooks.h"

#include "game/net/pn_fast_send.hpp"

// Proud::CFastSocket — same RVA as hook_send_2 (0xD567F0). Do not enable both.
// Send lives in pn_fast_send.cpp; recv stub kept for future full replacement.

using FastSocketRecvFn = int(__thiscall *)(void *self, int len);

static FastSocketRecvFn g_orig_fast_recv =
    reinterpret_cast<FastSocketRecvFn>(0xD56477);

int __fastcall ProudFastSocket_Recv(void *self, void * /*edx*/, int len) {
  return g_orig_fast_recv(self, len);
}

extern "C" void __declspec(naked) hook_fast_wsasend() {
  __asm { jmp PNFastSocket::send }
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
