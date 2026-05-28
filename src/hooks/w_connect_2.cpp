#include "game/net/proud_connect.hpp"
#include "target_hooks.h"

extern "C" void __declspec(naked) hook_w_connect_2() {
  __asm {
    jmp ProudConnect::w_connect_2
  }
}

HookStub g_target_w_connect_2 = {
    0x014314E0,
    (uint32_t)(uintptr_t)hook_w_connect_2,
    "hook_w_connect_2",
    {0},
    false,
    0x0143172F,
};
