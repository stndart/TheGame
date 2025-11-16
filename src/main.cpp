#include "console.h"
#include "crt/memory.h"
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
    CRT::init_CRT();
    create_console();

    // Initialize and apply all hooks
    HookManager::initialize();

    // HookManager::make_hook(g_target_w_strlen);

    HookManager::make_hook(g_target_w_connect_1);
    HookManager::make_hook(g_target_w_connect_2);
    HookManager::make_hook(g_target_w_connect_3);

    HookManager::make_hook(g_target_EnsureMStringBufferCapacity);
    HookManager::make_hook(g_target_EnsureWStringBufferCapacity);
    HookManager::make_hook(g_target_ConvertWideToMultiByte);
    HookManager::make_hook(g_target_ConvertMultiByteToWide);

    HookManager::make_hook(g_target_rstring_truncate);
    HookManager::make_hook(g_target_rstring_truncate_self);
    HookManager::make_hook(g_target_rstring_decrefcnt);
    HookManager::make_hook(g_target_rstring_copyonwrite);

    HookManager::make_hook(g_target_rstring_realloc);
    HookManager::make_hook(g_target_rstring_reserve);

    log_message("DLL loaded - hooks installed");
    break;

  case DLL_PROCESS_DETACH:
    log_message("DLL unloaded - hooks removed");
    if (log_file.is_open()) {
      log_file.close();
    }
    if (console_created) {
      FreeConsole();
    }

    HookManager::restore_all_hooks();
    break;
  }
  return TRUE;
};