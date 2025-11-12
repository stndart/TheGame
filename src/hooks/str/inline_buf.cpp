#include "console.h"
#include "target_hooks.h"

#include <game/engine/string.h>

extern "C" void __declspec(naked) hook_EnsureWStringBufferCapacity() {
  __asm {
    jmp EnsureWStringBufferCapacity
  }
}

HookStub g_target_EnsureWStringBufferCapacity = {
    0x5584C0,
    (uint32_t)(uintptr_t)hook_EnsureWStringBufferCapacity,
    "hook_EnsureWStringBufferCapacity",
    {0},
    false,
    0x5584C5,
};