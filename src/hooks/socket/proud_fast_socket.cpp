#include "target_hooks.h"

#include "ProudNet/FastSocket.hpp" // IWYU pragma: keep

extern "C" void __declspec(naked) hook_fast_wsasend() {
  __asm {
    jmp Proud::CFastSocket::send
  }
}

extern "C" void __declspec(naked) hook_fast_wsarecv() {
  __asm {
    jmp Proud::CFastSocket::recv
  }
}

HookStub g_target_fast_wsasend = {Proud::Rva::kFastSend,
                                  (uint32_t)(uintptr_t)hook_fast_wsasend,
                                  "hook_fast_wsasend",
                                  {0},
                                  false,
                                  0};

HookStub g_target_fast_wsarecv = {Proud::Rva::kFastRecv,
                                  (uint32_t)(uintptr_t)hook_fast_wsarecv,
                                  "hook_fast_wsarecv",
                                  {0},
                                  false,
                                  0};
