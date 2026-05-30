#include "target_hooks.h"

#include "ProudNet/FastSocket.hpp" // IWYU pragma: keep

extern "C" void __declspec(naked) hook_pn_recv_complete() {
  __asm {
    jmp Proud::CFastSocket::recv_complete
  }
}

HookStub g_target_pn_recv_complete = {
    Proud::Rva::kRecvComplete,
    (uint32_t)(uintptr_t)hook_pn_recv_complete,
    "hook_pn_recv_complete",
    {0},
    false,
    0};
