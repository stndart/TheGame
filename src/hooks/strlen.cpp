#include <windows.h>

#include "console.h"
#include "target_hooks.h"

unsigned int __cdecl handl_w_strlen(const char *str) {
  if (str)
    return strlen(str);
  return 0;
}

unsigned int __cdecl handle_w_strlen_log(void *retaddr, const char *str) {
  if (str) {
    size_t len = strlen(str);
    // logf("Call from 0x%p: len of [%s] at %p is %u", retaddr, str, str, len);
    return len;
  }
  // logf("Call from 0x%p: len of nullstr is 0", retaddr);
  return 0;
}

extern "C" void __declspec(naked) hook_w_strlen() {
  __asm {
    mov eax, [esp + 4]
    mov edx, [esp]
    push eax
    push edx
    call handle_w_strlen_log
    add esp, 8
    ret
  }

  // comment the previous if you want the logs disabled
  // __asm { jmp handle_w_strlen }
}

HookStub g_target_w_strlen = {
    0xCEE210, (uint32_t)(uintptr_t)hook_w_strlen, "hook_w_strlen", {0}, false,
    0xCEE216,
};
