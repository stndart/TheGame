#include "target_hooks.h"
#include <console.h>

#include "game/engine/String.h"

#include "WinSock2.h"

void __cdecl handle_sendto_1(void *_this, int *a2, int16_t a3, int a4,
                             int16_t a5, int a6) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  WSABUF *lpBuffers = reinterpret_cast<WSABUF *>(a2 + 5);
  if (a2[3] > 100)
    lpBuffers = reinterpret_cast<WSABUF *>(a2[1]);
  size_t n = a2[2];

  logf("handle_sendto_1: obj=%p, socket %u, a2[0]=%u, someflag=%u, buffers[%u] "
       "at %p, a3=%i, a4=%i, a5=%i, a6=%i",
       _this, sock, a2[0], a2[3], n, lpBuffers, a3, a4, a5, a6);

  for (size_t i = 0; i < n; ++i) {
    String s(lpBuffers[i].buf, lpBuffers[i].len);
    logf("handle_sendto_1 buf[%u/%u] of len %u: %s", i, n, lpBuffers[i].len,
         s.c_str());
  }
}

void __cdecl handle_sendto_2(void *_this, char *buf, int len, char a4, int a5,
                             int16_t a6) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  String s(buf, len);
  logf("handle_sendto_2: obj=%p, socket=%u, buf='%s' at %p, len=%u, a4=%p, "
       "a5=%u, a6=%u",
       _this, sock, s.c_str(), buf, len, a4, a5, a6);
}

void __cdecl handle_sendto_3(void *_this, char *a2, int a3, int16_t a4, int a5,
                             int16_t a6) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  logf("handle_sendto_3: obj=%p, socket=%u, a3=%i, a4=%i, a5=%i, "
       "a6=%i",
       _this, sock, a3, a4, a5, a6);
}

extern "C" void __declspec(naked) hook_sendto_1() {
  __asm {
    // esp += 0x0C
    pushad ; // esp += 0x20

    // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    push ecx;
    
    call handle_sendto_1 ;

    add esp, 24

    popad ;

    // Execute original instructions that were overwritten
    push -1
    push 0x015119B9

    // Jump back to original code after our patch
    jmp [g_target_sendto_1.return_addr]
  }
}

extern "C" void __declspec(naked) hook_sendto_2() {
  __asm {
    // esp += 0x0C
    pushad ; // esp += 0x20

    // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    push ecx;
    
    call handle_sendto_2 ;

    add esp, 24

    popad ;

    // Execute original instructions that were overwritten
    sub esp, 0x14
    mov eax, 0x17D4510

    // Jump back to original code after our patch
    jmp [g_target_sendto_2.return_addr]
  }
}

extern "C" void __declspec(naked) hook_sendto_3() {
  __asm {
    // esp += 0x0C
    pushad ; // esp += 0x20

    // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    mov eax, [esp + 0x30] ;
    push eax;
    push ecx;
    
    call handle_sendto_3 ;

    add esp, 24

    popad ;

    // Execute original instructions that were overwritten
    push -1
    push 0x15119E9

    // Jump back to original code after our patch
    jmp [g_target_sendto_3.return_addr]
  }
}

HookStub g_target_sendto_1 = {
    0xD57590, (uint32_t)(uintptr_t)hook_sendto_1, "hook_sendto_1", {0}, false,
    0xD57597,
};

HookStub g_target_sendto_2 = {
    0xD577B0, (uint32_t)(uintptr_t)hook_sendto_2, "hook_sendto_2", {0}, false,
    0xD577B8,
};

HookStub g_target_sendto_3 = {
    0xD57850, (uint32_t)(uintptr_t)hook_sendto_3, "hook_sendto_3", {0}, false,
    0xD57857,
};