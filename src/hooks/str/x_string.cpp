#include <windows.h>

#include "console.h"
#include "target_hooks.h"

#include <game/engine/rcstring.h>

void __cdecl handle_rstring_reserve(RefString *rstr, int required_size) {
  // logf("hook_rstring_reserve, req_size='%i'", required_size);
  log_string_structure(rstr, "hook_rstring_reserve");
}

extern "C" void __declspec(naked) hook_rstring_reserve() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24] ; // required_size 
    push eax
    push ecx ; // 'this' pointer (in ECX for __thiscall)
    
    call handle_rstring_reserve

    add esp, 8

    popad

        // Execute original instructions that were overwritten
    push esi
    mov esi, ecx
    mov eax, [esi] ;

    // Jump back to original code after our patch
    jmp [g_target_rstring_reserve.return_addr]
  }
}

HookStub g_target_rstring_reserve = {
    0xCF0020,
    (uint32_t)(uintptr_t)hook_rstring_reserve,
    "hook_rstring_reserve",
    {0},
    false,
    0xCF0025,
};

void __cdecl handle_rstring_realloc(const RefString *rstr, int new_size) {
  // logf("handle_rstring_realloc, new_size='%i'", new_size);
  log_string_structure(rstr, "handle_rstring_realloc");
}

extern "C" void __declspec(naked) hook_rstring_realloc() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24] ; // new_size 
    push eax
    push ecx ; // 'this' pointer (in ECX for __thiscall)
    
    call handle_rstring_realloc

    add esp, 8

    popad ;

    // Execute original instructions that were overwritten
    sub esp, 0x30
    push ebx
    mov ebx, ecx ;

    // Jump back to original code after our patch
    jmp [g_target_rstring_realloc.return_addr]
  }
}

HookStub g_target_rstring_realloc = {
    0xCEFEF0,
    (uint32_t)(uintptr_t)hook_rstring_realloc,
    "hook_rstring_realloc",
    {0},
    false,
    0xCEFEF6,
};