#include "diagnostics/handlers.hpp" // IWYU pragma: keep
#include "target_hooks.h"

extern "C" uint32_t g_entrypoint_first_call = 0x014ABE7E;

extern "C" void __declspec(naked) hook_entrypoint() {
  __asm {
    pushfd
    pushad
    call diagnostics_startup
    popad
    popfd;

    // Replay the 5-byte CALL overwritten at GAME.exe entrypoint 0x014AB273.
    call dword ptr [g_entrypoint_first_call]
    jmp [g_target_entrypoint.return_addr]
  }
}

HookStub g_target_entrypoint = {
    0x014AB273,
    (uint32_t)(uintptr_t)hook_entrypoint,
    "hook_entrypoint",
    {0},
    false,
    0x014AB278,
};
