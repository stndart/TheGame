#include "hook_manager.h"
#include "target_hooks.h"

// this is just for binding with GAME.exe
extern "C" __declspec(dllexport) void __cdecl A() {}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    // Disable thread notifications for better performance
    DisableThreadLibraryCalls(hModule);

    // Initialize and apply all hooks
    HookManager::initialize();
    HookManager::make_hook(g_target_w_connect_1);
    break;

  case DLL_PROCESS_DETACH:
    // Restore original code
    HookManager::restore_hook(g_target_w_connect_1);
    break;
  }
  return TRUE;
}