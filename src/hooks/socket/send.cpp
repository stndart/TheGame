#include "target_hooks.h"
#include <console.h>

#include "game/engine/String.h"

#include "WinSock2.h"

void __cdecl handle_send_1(void *_this, char *buf, int len) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  String s(buf, len);
  logf("handle_send_1: obj=%p, socket %p, buf='%s' at %p, len=%u", _this, sock,
       s.c_str(), buf, len);
}

void __cdecl handle_send_2(void *_this, char *buf, int len) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  String s(buf, len);
  logf("handle_send_2: obj=%p, socket %p, buf='%s' at %p, len=%u", _this, sock,
       s.c_str(), buf, len);
}

void __cdecl handle_send_3(void *_this, int *a2) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  WSABUF *lpBuffers = reinterpret_cast<WSABUF *>(a2 + 5);
  if (a2[3] > 100)
    lpBuffers = reinterpret_cast<WSABUF *>(a2[1]);

  size_t n = a2[2];
  logf("handle_send_3: obj=%p, socket %p, a2[0]=%u, someflag=%u, buffers[%u] "
       "at %p",
       _this, sock, a2[0], a2[3], n, lpBuffers);
  for (size_t i = 0; i < n; ++i) {
    String s(lpBuffers[i].buf, lpBuffers[i].len);
    // logf("handle_send_3 buf[%u/%u] of len %u: %s", i, n, lpBuffers[i].len,
    //      s.c_str());
  }
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
    mov eax, [esp + 0x28] ;
    mov ebx, [esp + 0x24] ;
    push eax;
    push ebx;
    push ecx;
    
    call handle_send_2 ;

    add esp, 12

    popad ;

    // Execute original instructions that were overwritten
    push -1;
    push 0x1511866;

    // Jump back to original code after our patch
    jmp [g_target_send_2.return_addr]
  }
}

extern "C" void __declspec(naked) hook_send_3() {
  __asm {
    // esp += 0x0C
    pushad ; // esp += 0x20

    // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x24] ;
    push eax;
    push ecx;
    
    call handle_send_3 ;

    add esp, 8

    popad ;

    // Execute original instructions that were overwritten
    push -1;
    push 0x01511889;

    // Jump back to original code after our patch
    jmp [g_target_send_3.return_addr]
  }
}

HookStub g_target_send_1 = {
    0xCF3290, (uint32_t)(uintptr_t)hook_send_1, "hook_send_1", {0}, false,
    0xCF3295,
};
HookStub g_target_send_2 = {
    0xD567F0, (uint32_t)(uintptr_t)hook_send_2, "hook_send_2", {0}, false,
    0xD567F7,
};
HookStub g_target_send_3 = {
    0xD569C0, (uint32_t)(uintptr_t)hook_send_3, "hook_send_3", {0}, false,
    0xD569C7,
};