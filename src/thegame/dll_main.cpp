#include "crt/memory.h"
#include "diagnostics/handlers.hpp"
#include "hook_manager.h"
#include "thegame/config.hpp"
#include "thegame/log.hpp"
#include "thegame/proud_db.hpp"

#include "system_hooks.h"
#include "target_hooks.h"

using thegame::logf;

extern "C" __declspec(dllexport) void __cdecl A() {}

static HMODULE g_dll_module = nullptr;

namespace {

void install_game_stage_hooks() {
  HookManager::make_hook(g_target_game_intro);
  HookManager::make_hook(g_target_game_login);
  HookManager::make_hook(g_target_game_server_select);
  HookManager::make_hook(g_target_game_main_menu);
  HookManager::make_hook(g_target_game_lobby);
  HookManager::make_hook(g_target_game_room_list);
  HookManager::make_hook(g_target_game_party_room);
  HookManager::make_hook(g_target_game_room);
  HookManager::make_hook(g_target_game_char_select);
  HookManager::make_hook(g_target_game_map_loading);
  HookManager::make_hook(g_target_game_in_game);
}

void install_syshooks() {
#if WS2_HOOKS
  HookManager::make_syshook(g_ws2_send, 0x01588B9C);
  HookManager::make_syshook(g_ws2_sendto, 0x01588C08);
  HookManager::make_syshook(g_ws2_recvfrom, 0x01588BA0);
  HookManager::make_syshook(g_ws2_wsarecv, 0x01588BF0);
  HookManager::make_syshook(g_ws2_wsarecvfrom, 0x01588BF4);
  HookManager::make_syshook(g_ws2_wsasend, 0x01588BF8);
  HookManager::make_syshook(g_ws2_wsasendto, 0x01588C04);
  HookManager::make_syshook(g_ws2_recv, 0x01588C14);
  HookManager::make_syshook(g_ws2_connect, 0x01588BEC);

  // PN reimpl calls WSASend/WSARecv through TheGame.dll imports (not GAME IAT).
  if (g_dll_module) {
    HookManager::hook_import(g_dll_module, "WS2_32.dll", "WSASend",
                             reinterpret_cast<void *>(wsasend_syshandle));
    HookManager::hook_import(g_dll_module, "WS2_32.dll", "WSARecv",
                             reinterpret_cast<void *>(wsarecv_syshandle));
  }
#endif
}

void install_engine_hooks() {
  HookManager::make_hook(g_target_w_strlen);
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
  HookManager::make_hook(g_target_rstring_truncateatfirst);
  HookManager::make_hook(g_target_rstring_trimleft);
  HookManager::make_hook(g_target_rstring_concatenate);
  HookManager::make_hook(g_target_rstring_concatenate_cstr);
  HookManager::make_hook(g_target_rstring_vformat);
  HookManager::make_hook(g_target_rstring_vformat_this);

  HookManager::make_hook(g_target_TCPSocket_connect);
  HookManager::make_hook(g_target_send_2);
  HookManager::make_hook(g_target_fast_wsarecv);
  HookManager::make_hook(g_target_pn_upnp);
  HookManager::make_hook(g_target_pn_recv_append);
  HookManager::make_hook(g_target_pn_select);
  HookManager::make_hook(g_target_pn_recv_complete);
  HookManager::make_hook(g_target_pn_fsm_state1);
  HookManager::make_hook(g_target_pn_fsm_state2);
  HookManager::make_hook(g_target_pn_fsm_state3);
  HookManager::make_hook(g_target_pn_net_client_factory);
  HookManager::make_hook(g_target_pn_net_client_ctor);
  HookManager::make_hook(g_target_pn_process_compressed);
  HookManager::make_hook(g_target_pn_process_encrypted);
  HookManager::make_hook(g_target_pn_process_proudnet_layer);
  HookManager::make_hook(g_target_pn_drain_receive_queue);
  HookManager::make_hook(g_target_pn_rmi_send);
  HookManager::make_hook(g_target_pn_game_rmi_send);
  HookManager::make_hook(g_target_pn_rmi_floor);
}

void install_hooks() {
  HookManager::initialize();

  if (!thegame::cfg.disable_entrypoint_hook)
    HookManager::make_hook(g_target_entrypoint);

  install_game_stage_hooks();

  if (!thegame::cfg.disable_syshooks)
    install_syshooks();

  install_engine_hooks();
}

void start_diagnostics_poll() {
  if (thegame::cfg.disable_entrypoint_hook) {
    if (thegame::cfg.pipes)
      Diagnostics::startup();
    return;
  }

  CreateThread(
      nullptr, 0,
      [](LPVOID) -> DWORD {
        for (int i = 0; i < 600; ++i) {
          if (Diagnostics::started())
            break;
          if (thegame::cfg.pipes)
            Diagnostics::startup();
          Sleep(100);
        }
        return 0;
      },
      nullptr, 0, nullptr);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    g_dll_module = module;
    DisableThreadLibraryCalls(module);
    CRT::init_CRT();
    thegame::init_config();
    thegame::create_console();
    thegame::prepare_logs();
    thegame::init_proud_db();
    thegame::log_boot_paths();

    if (thegame::cfg.disable_hooks) {
      logf("DLL loaded - no hooks (DISABLE_HOOKS)");
    } else {
      install_hooks();
      logf("DLL loaded - hooks installed");
      start_diagnostics_poll();
    }
    break;

  case DLL_PROCESS_DETACH:
    if (!thegame::cfg.disable_hooks) {
      logf("DLL unloaded - hooks removed");
      Diagnostics::teardown();
      HookManager::restore_all_hooks();
    } else {
      logf("DLL unloaded");
    }
    thegame::close_logs();
    if (thegame::console_created)
      FreeConsole();
    break;
  }
  return TRUE;
}
