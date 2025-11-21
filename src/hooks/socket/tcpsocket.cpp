#include "target_hooks.h"
#include <console.h>

#include <game/engine/TCPSocket.h>

void __cdecl handle_TCPSocket_connect(SOCKET *sock, LPCWSTR lpString,
                                      u_short hostshort, DWORD *esp) {
  logf("TCPSocket_connect: sock=%p, lpString='%ls', hostshort=%u, esp=%p", sock,
       lpString, hostshort, esp);
}

extern "C" void __declspec(naked) hook_TCPSocket_connect() {
  __asm {
    jmp TCPSocket::Connect
  }
}

extern "C" void __declspec(naked) hook_TCPSocket_connect_old() {
  __asm {
    pushad // esp += 0x20
    pushfd // esp += 0x04

            // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x2C] ; // hostshort
    mov ebx, [esp + 0x28] ; // lpString  
    push esp ;
    push eax
    push ebx
    push ecx ; // 'this' pointer (in ECX for __thiscall)
    
    call handle_TCPSocket_connect ;

    // Clean up stack (3 args * 4 bytes = 12)
    add esp, 16

    popfd
    popad ;

    // Execute original instructions that were overwritten
    push 0x0FFFFFFFF
    push 0x00D56220

    // Jump back to original code after our patch
    jmp [g_target_TCPSocket_connect.return_addr]
  }
}

HookStub g_target_TCPSocket_connect = {
    0xD56220,
    (uint32_t)(uintptr_t)hook_TCPSocket_connect,
    "hook_TCPSocket_connect",
    {0},
    false,
    0xD56227,
};
