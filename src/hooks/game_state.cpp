#include "diagnostics/handlers.hpp"
#include "target_hooks.h"

extern "C" void __cdecl diagnostics_game_state_intro() {
  Diagnostics::emit_game_state("intro");
}

extern "C" void __cdecl diagnostics_game_state_connecting() {
  Diagnostics::emit_game_state("connecting_to_server");
}

extern "C" void __cdecl diagnostics_game_state_shard_choice() {
  Diagnostics::emit_game_state("shard_choice");
}

extern "C" void __cdecl diagnostics_game_state_main_menu() {
  Diagnostics::emit_game_state("main_menu");
}

// CGameIntro::onPreProcess - switches to eGFxScene_Intro (14)
extern "C" void __declspec(naked) hook_game_intro() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_state_intro
    popfd
    popad

    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_intro.return_addr]
  }
}

// CGameLogin::onPreProcess - login GFX before server connection UI
extern "C" void __declspec(naked) hook_game_login() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_state_connecting
    popfd
    popad

    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_login.return_addr]
  }
}

// CGameServer::onPreProcess Begin - server / shard picker
extern "C" void __declspec(naked) hook_game_server_select() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_state_shard_choice
    popfd
    popad

    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_server_select.return_addr]
  }
}

// CGameServer::onPreProcess End - main menu ready (g_kMainMenu wired)
extern "C" void __declspec(naked) hook_game_main_menu() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_state_main_menu
    popfd
    popad

        // Original: mov ecx, offset aCGameServerOnPreProcessEnd (0x16B7288)
    mov ecx, 0x16B7288
    jmp [g_target_game_main_menu.return_addr]
  }
}

HookStub g_target_game_intro = {
    0x42A010,
    (uint32_t)(uintptr_t)hook_game_intro,
    "hook_game_intro",
    {0},
    false,
    0x42A015,
};

HookStub g_target_game_login = {
    0x42B280,
    (uint32_t)(uintptr_t)hook_game_login,
    "hook_game_login",
    {0},
    false,
    0x42B285,
};

HookStub g_target_game_server_select = {
    0x4345B0,
    (uint32_t)(uintptr_t)hook_game_server_select,
    "hook_game_server_select",
    {0},
    false,
    0x4345B5,
};

HookStub g_target_game_main_menu = {
    0x4347CC,
    (uint32_t)(uintptr_t)hook_game_main_menu,
    "hook_game_main_menu",
    {0},
    false,
    0x4347D1,
};
