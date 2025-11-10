#include "console.h"
#include "helpers/strhelp.h"
#include "target_hooks.h"

#include <basetsd.h>
#include <debugapi.h>

typedef UINT_PTR SOCKET;

void __cdecl handle_w_connect_1(SOCKET *sock, LPCWSTR lpString,
                                uint16_t hostshort) {
  logf("w_connect_1: sock=%p, lpString='%s', hostshort=%u", sock,
       wstring_to_string(lpString).c_str(), hostshort);
}

extern "C" void __declspec(naked) hook_w_connect_1() {
  __asm {
    pushad // esp += 0x20
    pushfd // esp += 0x04

            // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x2C] ; // hostshort
    mov ebx, [esp + 0x28] ; // lpString  
    push eax
    push ebx
    push ecx ; // 'this' pointer (in ECX for __thiscall)
    
    call handle_w_connect_1

        // Clean up stack (3 args * 4 bytes = 12)
    add esp, 12

    popfd
    popad

        // Execute original instructions that were overwritten
    push 0x0FFFFFFFF
    push 0x00D56220

    // Jump back to original code after our patch
    jmp [g_target_w_connect_1.return_addr]
  }
}

HookStub g_target_w_connect_1 = {
    0xD56220,
    (uint32_t)(uintptr_t)hook_w_connect_1,
    "hook_w_connect_1",
    {0},
    false,
    0xD56227,
};