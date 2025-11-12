#include "console.h"
#include "target_hooks.h"

#include <game/engine/multibyteholder.h>

extern "C" void __declspec(naked) hook_ConvertWideToMultiByte() {
  __asm {
    jmp MultiByteHolder::ConvertWideToMultiByte
  }
}

HookStub g_target_ConvertWideToMultiByte = {
    0x535740,
    (uint32_t)(uintptr_t)hook_ConvertWideToMultiByte,
    "hook_w_connect_1",
    {0},
    false,
    0x535746,
};