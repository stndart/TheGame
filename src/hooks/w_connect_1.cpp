#include "target_hooks.h"

void log_w_connect_1() {
  OutputDebugStringA("hook_w_connect_1 fired!");
  // You can add more complex logic here
}

extern "C" void __declspec(naked) hook_w_connect_1() {
  // Execute original instructions that were overwritten
  __asm {
        pushad
        pushfd
        call log_w_connect_1
        popfd
        popad

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