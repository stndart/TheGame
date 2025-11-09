#include "target_hooks.h"

#include <basetsd.h>
#include <debugapi.h>
#include <sstream>

typedef UINT_PTR SOCKET;

void __cdecl handle_w_connect_1(SOCKET *sock, LPCWSTR lpString,
                                uint16_t hostshort) {
  OutputDebugStringA("hook_w_connect_1 fired!");
  std::stringstream str;
  str << "Sock addr " << std::hex << sock << ", lp: " << lpString
      << ", h: " << hostshort << "\n";
  OutputDebugStringA(str.str().c_str());
}

extern "C" void __declspec(naked) hook_w_connect_1() {
  __asm {
    pushad
    pushfd

        // Push arguments in reverse order (__cdecl convention)
    mov eax, [esp + 0x28] ; // hostshort (adjust offset based on your stack)
    push eax
    mov eax, [esp + 0x2C] ; // lpString  
    push eax
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
    jmp [g_target_w_connect_1.return_rva]
  }
}

// Define your stubs with actual RVAs from your target EXE
HookStub g_target_w_connect_1 = {
    0x956220, (uint32_t)(uintptr_t)hook_w_connect_1, "hook_w_connect_1", {0},
    false,
    0xD56227, // + 0x400000
};