#include <windows.h>

#include "hook_manager.h"
#include "target_hooks.h"

// Define your stubs with actual RVAs from your target EXE
HookStub g_target_example_method = {
    0x156220, // RVA in target EXE - adjust this!
    0x1000,   // RVA of hook_example_method in this DLL - adjust after
              // compilation!
    "target_example_method",
    {0},
    false};

HookStub g_another_target_method = {0x157000, // Another RVA - adjust!
                                    0x1100,   // Another hook RVA - adjust!
                                    "another_target_method",
                                    {0},
                                    false};

// Your actual hook implementations
extern "C" void __fastcall hook_example_method() {
  // Do your pre-work here
  OutputDebugStringA("hook_example_method fired!");

  // Execute original instructions that were overwritten
  __asm {
        push -1
        push 0x00D56220 // Original SEH handler address
        mov eax, dword ptr fs:[0]
        push eax

         // Jump back to original code after our patch
        mov eax, 0x00D56231 // Address after the 5-byte JMP we overwrote
        jmp eax
  }
}

extern "C" void __fastcall hook_another_target_method() {
  OutputDebugStringA("hook_another_target_method fired!");

  // Your hook logic here
  __asm {
    // Replicate original instructions
    // ...

    // Jump back
        mov eax, 0x00D57005 // Adjust this address
        jmp eax
  }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    // Disable thread notifications for better performance
    DisableThreadLibraryCalls(hModule);

    // Initialize and apply all hooks
    HookManager::initialize();
    HookManager::make_hook(g_target_example_method);
    HookManager::make_hook(g_another_target_method);
    break;

  case DLL_PROCESS_DETACH:
    // Restore original code
    HookManager::restore_hook(g_target_example_method);
    HookManager::restore_hook(g_another_target_method);
    break;
  }
  return TRUE;
}