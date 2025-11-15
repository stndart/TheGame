#include "console.h"
#include "hook_manager.h"
#include "target_hooks.h"

#include <game/engine/String.h>

extern "C" void __declspec(naked) hook_rstring_truncate() {
  __asm {
      jmp String::Truncate;
  }
}

extern "C" void __declspec(naked) hook_rstring_truncate_self() {
  __asm {
      jmp String::TruncateSelf;
  }
}

extern "C" void __declspec(naked) hook_rstring_decrefcnt() {
  __asm {
    jmp String::DecRefCount;
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

HookStub g_target_rstring_decrefcnt = {
    0x9FCAF0,
    (uint32_t)(uintptr_t)hook_rstring_decrefcnt,
    "hook_rstring_decrefcnt",
    {0},
    false,
    0x9FCAF5,
};
