#include "console.h"
#include "target_hooks.h"

#include <game/engine/rcstring.h>

extern "C" void __declspec(naked) hook_rstring_truncate() {
  __asm {
      jmp RefString::truncate;
  }
}

extern "C" void __declspec(naked) hook_rstring_truncate_self() {
  __asm {
      jmp RefString::truncate_self;
  }
}

HookStub g_target_rstring_truncate = {
    0xCEFCC0,
    (uint32_t)(uintptr_t)hook_rstring_truncate,
    "hook_rstring_truncate",
    {0},
    false,
    0xCEFCC6,
};

HookStub g_target_rstring_truncate_self = {
    0xD5A7E0,
    (uint32_t)(uintptr_t)hook_rstring_truncate_self,
    "hook_rstring_truncate_self",
    {0},
    false,
    0xD5A7E5,
};