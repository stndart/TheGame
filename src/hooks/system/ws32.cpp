#include "hook_manager.h"
#include "system_hooks.h"
#include <console.h>

// #include "WinSock2.h"
#include <windows.h>

#define WS32_SYSLOGS 1

void __cdecl log_parg(void *retaddr, void *p) {
  if (WS32_SYSLOGS)
    logf("Call from 0x%p with arg 0x%p", retaddr, p);
}

void __cdecl log_parg_n(int i, void *retaddr, void *p) {
  if (WS32_SYSLOGS)
    logf("Call[%i] from 0x%p with arg 0x%p", i, retaddr, p);
}

extern "C" void __declspec(naked) send_syshandle() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24]; // Socket
    mov ebx, [esp + 0x20]; // retaddr
    push eax
    push ebx
    push 1
    call log_parg_n
    add esp, 12;

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
    push 2
    call log_parg_n
    add esp, 12;

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
    push 3
    call log_parg_n
    add esp, 12;

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
    push 4
    call log_parg_n
    add esp, 12;

    popad;
    
    jmp g_ws2_wsasendto.sym_addr
  }

  // comment the previous if you want the logs disabled
  __asm { jmp g_ws2_wsasendto.sym_addr }
}

extern "C" void __declspec(naked) connect_syshandle() {
  __asm {
    pushad // esp += 0x20

    mov eax, [esp + 0x24]; // Socket
    mov ebx, [esp + 0x20]; // retaddr
    push eax
    push ebx
    push 5
    call log_parg_n
    add esp, 12;

    popad;
    
    jmp g_ws2_connect.sym_addr
  }

  // comment the previous if you want the logs disabled
  __asm { jmp g_ws2_connect.sym_addr }
}

SysHookStub g_ws2_send = {"ws2_32.dll", "send", send_syshandle};          // 1
SysHookStub g_ws2_sendto = {"ws2_32.dll", "sendto", sendto_syshandle};    // 2
SysHookStub g_ws2_wsasend = {"ws2_32.dll", "WSASend", wsasend_syshandle}; // 3
SysHookStub g_ws2_wsasendto = {"ws2_32.dll", "WSASendTo",
                               wsasendto_syshandle};                      // 4
SysHookStub g_ws2_connect = {"ws2_32.dll", "connect", connect_syshandle}; // 5