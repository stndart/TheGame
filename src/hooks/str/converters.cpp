#include "console.h"
#include "target_hooks.h"

#include <game/engine/StringConverters.h>

extern "C" void __declspec(naked) hook_ConvertWideToMultiByte() {
  __asm {
    jmp MultiByteHolder::ConvertWideToMultiByte
  }
}

HookStub g_target_ConvertWideToMultiByte = {
    0x535740,
    (uint32_t)(uintptr_t)hook_ConvertWideToMultiByte,
    "hook_ConvertWideToMultiByte",
    {0},
    false,
    0x535746,
};

extern "C" void __declspec(naked) hook_ConvertMultiByteToWide() {
  __asm {
    jmp WideStringHolder::ConvertMultiByteToWide
  }
}

HookStub g_target_ConvertMultiByteToWide = {
    0x578C90,
    (uint32_t)(uintptr_t)hook_ConvertMultiByteToWide,
    "hook_ConvertMultiByteToWide",
    {0},
    false,
    0x578C96,
};