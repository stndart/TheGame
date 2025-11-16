#include "console.h"
#include "target_hooks.h"

void __cdecl log_str(void **handle) {
  void *nullstr_hardcoded = *reinterpret_cast<void **>(0x017B75E4);
  if (handle == nullptr) {
    logf("log: null at %p", handle);
  } else {
    void *ptr = *handle;
    if (ptr == nullstr_hardcoded || ptr == nullptr) {
      logf("log: nullstr at %p at %p", ptr, handle);
    } else {
      int *header = reinterpret_cast<int *>(reinterpret_cast<char *>(ptr) - 8);
      if (header) {
        int len = header[0];
        size_t cnt = header[1];
        size_t cnt2 = cnt > 15 ? 15 : cnt;
        char str[16];
        str[15] = 0;
        memcpy(str, ptr, cnt2);
        logf("log: str at %p with len %i and refcnt %i starts as %s at %p", ptr,
             len, cnt, str, handle);
      } else {
        logf("log: broken header at %p at %p", ptr, handle);
      }
    }
  }
}

extern "C" void __declspec(naked) log_structure_after() {
  __asm {
    pushad;

    push esi;
    call log_str;
    add esp, 4;

    popad;
    // the rest of original code
    pop esi;
    test eax, eax;
    jne $+12;
    mov eax, ds:[0x017B75E4];
    
    jmp [g_target_logstr_end.return_addr]
  }
}

HookStub g_target_logstr_end = {
    0xCF0089,
    (uint32_t)(uintptr_t)log_structure_after,
    "log_structure_after",
    {0},
    false,
    0xCF0093,
};

extern "C" void __declspec(naked) log_structure_before() {
  __asm {
    push ecx;
    call log_str;
    pop ecx;

    // the rest of original code
    push esi;
    mov esi, ecx
    mov eax, ds:[esi]

    jmp [g_target_logstr_start.return_addr]
  }
}

HookStub g_target_logstr_start = {
    0xCF0020,
    (uint32_t)(uintptr_t)log_structure_before,
    "log_structure_before",
    {0},
    false,
    0xCF0025,
};