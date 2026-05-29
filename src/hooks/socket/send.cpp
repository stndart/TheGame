#include "target_hooks.h"

#include "ProudNet/FastSocket.hpp"

// w_wsasend_1 @ 0xD567F0 - full replacement (Proud::CFastSocket::send).
extern "C" void __declspec(naked) hook_send_2() {
  __asm {
    jmp Proud::CFastSocket::send
  }
}

HookStub g_target_send_2 = {
    0xD567F0, (uint32_t)(uintptr_t)hook_send_2, "hook_send_2", {0}, false, 0,
};
