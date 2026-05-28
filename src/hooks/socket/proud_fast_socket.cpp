#include "target_hooks.h"

#include "game/net/pn_fast_socket.hpp"

extern "C" void __declspec(naked) hook_fast_wsasend() {
  __asm {
    jmp PNFastSocket::send
  }
}

extern "C" void __declspec(naked) hook_fast_wsarecv() {
  __asm {
    jmp PNFastSocket::recv
  }
}

HookStub g_target_fast_wsasend = {pn::rva::kFastSend,
                                  (uint32_t)(uintptr_t)hook_fast_wsasend,
                                  "hook_fast_wsasend",
                                  {0},
                                  false,
                                  0};

HookStub g_target_fast_wsarecv = {pn::rva::kFastRecv,
                                  (uint32_t)(uintptr_t)hook_fast_wsarecv,
                                  "hook_fast_wsarecv",
                                  {0},
                                  false,
                                  0};
