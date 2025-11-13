#include "console.h"
#include "target_hooks.h"

#include <game/engine/rcstring.h>

extern "C" void __declspec(naked) hook_rstring_truncate() {
  __asm {
      jmp RefString::truncate;
  }

  __asm {
    pushad; // esp += 0x20

    mov eax, [esp + 0x24]; // new_size
    push eax;

    call RefString::truncate;

    // add esp, 4;

    popad;

    // Execute original instructions that were overwritten
    mov eax, [esp + 4];
    xor edx, edx;

    // Jump back to original code after our patch
    jmp[g_target_rstring_truncate.return_addr]
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