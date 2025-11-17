// #include "console.h"
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

extern "C" void __declspec(naked) hook_rstring_copyonwrite() {
  __asm {
    jmp String::CopyOnWrite;
  }
}

extern "C" void __declspec(naked) hook_rstring_reserve() {
  __asm {
    jmp String::Reserve;
  }
}

extern "C" void __declspec(naked) hook_rstring_realloc() {
  __asm {
    jmp String::Realloc;
    
    sub esp, 0x30
    push ebx
    mov ebx, ecx ;

    // Jump back to original code after our patch
    jmp [g_target_rstring_realloc.return_addr]
  }
}

extern "C" void __declspec(naked) hook_rstring_truncateatfirst() {
  __asm {
    jmp String::TruncateAtFirstOccurrence;
  }
}

extern "C" void __declspec(naked) hook_rstring_trimleft() {
  __asm {
    jmp String::TrimLeft;
  }
}

extern "C" void __declspec(naked) hook_rstring_concatenate() {
  __asm {
    jmp String::Concatenate;

    pushad;
    mov eax, [esp + 0x24]; // String* other
    push eax
    call String::Concatenate;
    add esp, 4 // CAREFUL (maybe bug)
    popad;
    ret;
    
    push ebp
    push edi
    mov edi, [esp+12]

    // Jump back to original code after our patch
    jmp [g_target_rstring_concatenate.return_addr]
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

HookStub g_target_rstring_copyonwrite = {
    0xCEFDF0,
    (uint32_t)(uintptr_t)hook_rstring_copyonwrite,
    "hook_rstring_copyonwrite",
    {0},
    false,
    0xCEFDF5,
};

HookStub g_target_rstring_reserve = {
    0xCF0020,
    (uint32_t)(uintptr_t)hook_rstring_reserve,
    "hook_rstring_reserve",
    {0},
    false,
    0xCF0025,
};

HookStub g_target_rstring_realloc = {
    0xCEFEF0,
    (uint32_t)(uintptr_t)hook_rstring_realloc,
    "hook_rstring_realloc",
    {0},
    false,
    0xCEFEF6,
};

HookStub g_target_rstring_truncateatfirst = {
    0xCF2D90,
    (uint32_t)(uintptr_t)hook_rstring_truncateatfirst,
    "hook_rstring_truncateatfirst",
    {0},
    false,
    0xCF2D96,
};

HookStub g_target_rstring_trimleft = {
    0xCF2CD0,
    (uint32_t)(uintptr_t)hook_rstring_trimleft,
    "hook_rstring_trimleft",
    {0},
    false,
    0xCF2CD8,
};

HookStub g_target_rstring_concatenate = {
    0xCF1A90,
    (uint32_t)(uintptr_t)hook_rstring_concatenate,
    "hook_rstring_concatenate",
    {0},
    false,
    0xCF1A96,
};