#include "game/net/proud_connect.hpp"
#include "target_hooks.h"

extern "C" void __declspec(naked) hook_w_connect_3() {
  __asm {
    jmp ProudConnect::Socket::w_connect_3
  }
}

HookStub g_target_w_connect_3 = {
    0x014F6DC0,
    (uint32_t)(uintptr_t)hook_w_connect_3,
    "hook_w_connect_3",
    {0},
    false,
    0x014F6DDB,
};
