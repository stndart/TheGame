#include "target_hooks.h"
#include <console.h>

#include "game/engine/String.h"

#include "WinSock2.h"

void __cdecl handle_send_1(SOCKET *sock, char *buf, int len) {
  String s(buf, len);
  logf("handle_send_1: sockobj=%p, buf='%s' at %p, len=%u", sock, s.c_str(),
       buf, len);
}

void __cdecl handle_send_2(SOCKET *sock, char *buf, int len) {
  String s(buf, len);
  logf("handle_send_2: sockobj=%p, buf='%s' at %p, len=%u", sock, s.c_str(),
       buf, len);
}

void __cdecl handle_send_3(SOCKET *sock, char *buf, int len) {
  String s(buf, len);
  logf("handle_send_3: sockobj=%p, buf='%s' at %p, len=%u", sock, s.c_str(),
       buf, len);
}
extern "C" void __declspec(naked) hook_send_1() {
  __asm {
    pushad // esp += 0x20

        // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x28] ;
    mov ebx, [esp + 0x24] ;
    push eax;
    push ebx;
    push ecx;
    
    call handle_send_1 ;

    add esp, 12

    popad ;

    // Execute original instructions that were overwritten
    mov eax, [ecx+8] ;
    test eax, eax ;

    // Jump back to original code after our patch
    jmp [g_target_send_1.return_addr]
  }
}

extern "C" void __declspec(naked) hook_send_2() {
  __asm {
    // esp += 0x0C
    pushad ; // esp += 0x20

    // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x34] ;
    mov ebx, [esp + 0x30] ;
    push eax;
    push ebx;
    push ecx;
    
    call handle_send_2 ;

    add esp, 12

    popad ;

    // Execute original instructions that were overwritten
    sub esp, 0x10
    push ebx
    push ebp
    push esi
    push edi ;

    // Jump back to original code after our patch
    jmp [g_target_send_2.return_addr]
  }
}

extern "C" void __declspec(naked) hook_send_3() {
  __asm {
    // esp += 0x0C
    pushad ; // esp += 0x20

    // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x34] ;
    mov ebx, [esp + 0x30] ;
    push eax;
    push ebx;
    push ecx;
    
    call handle_send_3 ;

    add esp, 12

    popad ;

    // Execute original instructions that were overwritten
    sub esp, 8
    push ebx
    push ebp
    push esi
    push edi ;

    // Jump back to original code after our patch
    jmp [g_target_send_3.return_addr]
  }
}

HookStub g_target_send_1 = {
    0xCF3290, (uint32_t)(uintptr_t)hook_send_1, "hook_send_1", {0}, false,
    0xCF3295,
};
HookStub g_target_send_2 = {
    0xD567FE, (uint32_t)(uintptr_t)hook_send_2, "hook_send_2", {0}, false,
    0xD56805,
};
HookStub g_target_send_3 = {
    0xD569CE, (uint32_t)(uintptr_t)hook_send_3, "hook_send_3", {0}, false,
    0xD569D5,
};