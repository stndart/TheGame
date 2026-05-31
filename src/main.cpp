#include "ProudNet/TcpTrace.hpp"
#include "console.h"
#include "crt/memory.h"
#include "diagnostics/handlers.hpp"
#include "hook_manager.h"

#include "system_hooks.h"
#include "target_hooks.h"

// this is just for binding with GAME.exe
extern "C" __declspec(dllexport) void __cdecl A() {}

static HMODULE g_dll_module = nullptr;

namespace {

void install_game_state_hooks(bool map_loading, bool in_game) {
  HookManager::make_hook(g_target_game_intro);
  HookManager::make_hook(g_target_game_login);
  HookManager::make_hook(g_target_game_server_select);
  HookManager::make_hook(g_target_game_main_menu);
  // Real post-server-select screen stages (CGameXxx::onPreProcess).
  HookManager::make_hook(g_target_game_lobby);
  HookManager::make_hook(g_target_game_room_list);
  HookManager::make_hook(g_target_game_party_room);
  HookManager::make_hook(g_target_game_room);
  HookManager::make_hook(g_target_game_char_select);
  if (map_loading)
    HookManager::make_hook(g_target_game_map_loading);
  if (in_game)
    HookManager::make_hook(g_target_game_in_game);
}

void install_game_state_hooks() {
  install_game_state_hooks(true, true);
}

void install_full_hooks() {
  HookManager::make_syshook(g_ws2_send, 0x01588B9C);
  HookManager::make_syshook(g_ws2_sendto, 0x01588C08);
  HookManager::make_syshook(g_ws2_wsasend, 0x01588BF8);
  HookManager::make_syshook(g_ws2_wsasendto, 0x01588C04);
  HookManager::make_syshook(g_ws2_connect, 0x01588BEC);
  HookManager::hook_import(GetModuleHandle(nullptr), "WS2_32.dll", "connect",
                           reinterpret_cast<void *>(connect_syshandle));
  HookManager::hook_import(g_dll_module, "WS2_32.dll", "connect",
                           reinterpret_cast<void *>(connect_syshandle));

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

  // TCPSocket_connect only; TX proudnet-tcp via WSASend IAT (ws32.cpp).
  HookManager::make_hook(g_target_TCPSocket_connect);
  // HookManager::make_hook(g_target_TCPSocket_send);
  HookManager::make_hook(g_target_send_2);
  // HookManager::make_hook(g_target_fast_wsasend); // same RVA as send_2
  HookManager::make_hook(g_target_fast_wsarecv);
  // HookManager::make_hook(g_target_recv_1);
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
  // sub_D65940 - OnMessageReceived trace; SEH tail-jump resume 0xD65947.
  HookManager::make_hook(g_target_pn_drain_receive_queue);
  // IRmiProxy::RmiSend trace - framework heartbeat ids only (SEH resume
  // 0xD5C5E7).
  HookManager::make_hook(g_target_pn_rmi_send);
  // GAME application RMI send capture: sub_65AEA0 (explicit id) + sub_A0B290
  // floor.
  HookManager::make_hook(g_target_pn_game_rmi_send);
  HookManager::make_hook(g_target_pn_rmi_floor);
  // Recv framer sub_D84BB0: restore_hook + __thiscall shim (SEH prologue; not
  // RVA+5). HookManager::make_hook(g_target_pn_tcp_frame_recv); Send framer
  // sub_D84970 @ entry only - resume 0xD84977, not RVA+5.
  // HookManager::make_hook(g_target_pn_tcp_frame_send);
}

void install_diag_net_hooks() {
  HookManager::make_hook(g_target_pn_process_proudnet_layer);
  HookManager::make_hook(g_target_pn_game_rmi_send);
  HookManager::make_hook(g_target_pn_rmi_floor);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    g_dll_module = module;
    DisableThreadLibraryCalls(module);
    CRT::init_CRT();
#ifndef THEGAME_NO_CONSOLE
    create_console();
#endif

#ifdef THEGAME_NO_HOOKS
    log_message("DLL loaded - no hooks (THEGAME_NO_HOOKS)");
#elif defined(THEGAME_HOOKS_ENTRYPOINT_ONLY)
    HookManager::initialize();
    HookManager::make_hook(g_target_entrypoint);
    log_message("DLL loaded - hooks installed (entrypoint-only)");
#elif defined(THEGAME_HOOKS_DIAG_MAP_ONLY)
    HookManager::initialize();
    HookManager::make_hook(g_target_entrypoint);
    HookManager::make_hook(g_target_game_map_loading);
    log_message("DLL loaded - hooks installed (diag-map-only)");
#elif defined(THEGAME_HOOKS_DIAG_NO_MAP)
    HookManager::initialize();
    HookManager::make_hook(g_target_entrypoint);
    install_game_state_hooks(false, false);
    log_message("DLL loaded - hooks installed (diag-no-map)");
#elif defined(THEGAME_HOOKS_DIAG_ONLY)
    HookManager::initialize();
    HookManager::make_hook(g_target_entrypoint);
    install_game_state_hooks();
    log_message("DLL loaded - hooks installed (diag-only)");
#elif defined(THEGAME_HOOKS_DIAG_NET)
    HookManager::initialize();
    HookManager::make_hook(g_target_entrypoint);
    install_game_state_hooks();
    install_diag_net_hooks();
    log_message("DLL loaded - hooks installed (diag-net)");
#else
    HookManager::initialize();
    HookManager::make_hook(g_target_entrypoint);
    install_game_state_hooks();
    install_full_hooks();
    log_message("DLL loaded - hooks installed");
#endif

#ifndef THEGAME_NO_HOOKS
    CreateThread(
        nullptr, 0,
        [](LPVOID) -> DWORD {
          for (int i = 0; i < 600; ++i) {
            if (Diagnostics::started())
              break;
            Diagnostics::startup();
            Sleep(100);
          }
          return 0;
        },
        nullptr, 0, nullptr);
#endif

    break;

  case DLL_PROCESS_DETACH:
#ifndef THEGAME_NO_HOOKS
    log_message("DLL unloaded - hooks removed");
    Diagnostics::teardown();
#else
    log_message("DLL unloaded");
#endif
    if (log_file.is_open()) {
      log_file.close();
    }
    Proud::TcpTrace::close_log_file();
#ifndef THEGAME_NO_CONSOLE
    if (console_created) {
      FreeConsole();
    }
#endif

#ifndef THEGAME_NO_HOOKS
    HookManager::restore_all_hooks();
#endif
    break;
  }
  return TRUE;
};
