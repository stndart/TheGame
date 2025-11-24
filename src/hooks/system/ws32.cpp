#include "system_hooks.h"
#include <console.h>

// #include "WinSock2.h"
#include <windows.h>

#define WS32_SYSLOGS 0

void __cdecl log_parg(void *retaddr, void *p) {
  if (WS32_SYSLOGS)
    logf("Call from 0x%p with arg 0x%p", retaddr, p);
}

extern "C" void __declspec(naked) send_syshandle() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24]; // Socket
    mov ebx, [esp + 0x20]; // retaddr
    push eax
    push ebx
    call log_parg
    add esp, 8;

    popad;
    
    jmp g_ws2_send.sym_addr
  }

  // comment the previous if you want the logs disabled
  __asm { jmp g_ws2_send.sym_addr }
}

extern "C" void __declspec(naked) sendto_syshandle() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24]; // Socket
    mov ebx, [esp + 0x20]; // retaddr
    push eax
    push ebx
    call log_parg
    add esp, 8;

    popad;
    
    jmp g_ws2_sendto.sym_addr
  }

  // comment the previous if you want the logs disabled
  __asm { jmp g_ws2_sendto.sym_addr }
}

extern "C" void __declspec(naked) wsasend_syshandle() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24]; // Socket
    mov ebx, [esp + 0x20]; // retaddr
    push eax
    push ebx
    call log_parg
    add esp, 8;

    popad;
    
    jmp g_ws2_wsasend.sym_addr
  }

  // comment the previous if you want the logs disabled
  __asm { jmp g_ws2_wsasend.sym_addr }
}

extern "C" void __declspec(naked) wsasendto_syshandle() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24]; // Socket
    mov ebx, [esp + 0x20]; // retaddr
    push eax
    push ebx
    call log_parg
    add esp, 8;

    popad;
    
    jmp g_ws2_wsasendto.sym_addr
  }

  // comment the previous if you want the logs disabled
  __asm { jmp g_ws2_wsasendto.sym_addr }
}

SysHookStub g_ws2_send = {"ws2_32.dll", "send", send_syshandle};
SysHookStub g_ws2_sendto = {"ws2_32.dll", "sendto", sendto_syshandle};
SysHookStub g_ws2_wsasend = {"ws2_32.dll", "WSASend", wsasend_syshandle};
SysHookStub g_ws2_wsasendto = {"ws2_32.dll", "WSASendTo", wsasendto_syshandle};