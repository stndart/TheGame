#include "console.h"
#include "target_hooks.h"

#include "WinSock2.h"

void __cdecl handle_w_connect_2(SOCKET *sock, DWORD *a, int b, int c) {
  logf("w_connect_2: sock=%p, *a='%s', b=%i, c=%i", sock, a, b, c);
}

extern "C" void __declspec(naked) hook_w_connect_2() {
  __asm {
    pushad // esp += 0x20
    pushfd // esp += 0x04

            // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x2C] ; // a4
    mov ebx, [esp + 0x28] ; // a3  
    push eax
    push ebx
    push edx
    push ecx
    
    call handle_w_connect_2

    add esp, 16

    popfd
    popad

        // Execute original instructions that were overwritten
    sub esp, 0xA4

    // Jump back to original code after our patch
    jmp [g_target_w_connect_2.return_addr]
  }
}

HookStub g_target_w_connect_2 = {
    0x14314E0,
    (uint32_t)(uintptr_t)hook_w_connect_2,
    "hook_w_connect_2",
    {0},
    false,
    0x14314E6,
};