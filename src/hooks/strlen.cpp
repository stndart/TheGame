#include <windows.h>

#include "target_hooks.h"

unsigned int __cdecl handle_w_strlen(const char *str) {
  if (str)
    return strlen(str);
  return 0;
}

extern "C" void __declspec(naked) hook_w_strlen() {
  __asm { jmp handle_w_strlen }
}

HookStub g_target_w_strlen = {
    0xCEE210, (uint32_t)(uintptr_t)hook_w_strlen, "hook_w_strlen", {0}, false,
    0xCEE216,
};
