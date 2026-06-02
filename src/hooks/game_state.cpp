#include "RMI/Nav.hpp"
#include "target_hooks.h"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

namespace {

// Handler-pipe nav must run even when DISABLE_AUTONAV is set in the environment
// (legacy flag). Stage hooks pump when onPreProcess runs; ctl commands also use
// NavSchedulePump (window message) because shard UI does not tick every frame.
void nav_pump(const char *phase) { Rmi::NavPump(phase); }

} // namespace

extern "C" void __cdecl diagnostics_game_stage_intro() {
  thegame::stagef("intro");
  nav_pump("intro");
}

extern "C" void __cdecl diagnostics_game_stage_login() {
  thegame::stagef("login");
  nav_pump("login");
}

// CGameServer::onPreProcess begin @ 0x4345B0 (no ctl stage emit).
extern "C" void __cdecl diagnostics_game_stage_server_begin() {
  nav_pump("server_begin");
}

// CGameServer::onPreProcess end @ 0x4347CC — shard picker UI is active.
extern "C" void __cdecl diagnostics_game_stage_shard_select() {
  thegame::stagef("shard_select");
  nav_pump("shard_select");
}

extern "C" void __cdecl diagnostics_game_stage_lobby() {
  thegame::stagef("lobby");
  nav_pump("lobby");
}

extern "C" void __cdecl diagnostics_game_stage_room_list() {
  thegame::stagef("room_list");
  nav_pump("room_list");
}

extern "C" void __cdecl diagnostics_game_stage_party_room() {
  thegame::stagef("party_room");
  nav_pump("party_room");
}

extern "C" void __cdecl diagnostics_game_stage_room() {
  thegame::stagef("room");
  nav_pump("room");
}

extern "C" void __cdecl diagnostics_game_stage_char_select() {
  thegame::stagef("char_select");
  nav_pump("char_select");
}

extern "C" void __cdecl diagnostics_game_stage_map_loading() {
  thegame::stagef("map_loading");
  nav_pump("map_loading");
}

extern "C" void __cdecl diagnostics_game_stage_in_game() {
  thegame::stagef("in_game");
  nav_pump("in_game");
}

// CGameIntro::onPreProcess - switches to eGFxScene_Intro (14)
extern "C" void __declspec(naked) hook_game_intro() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_intro
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
    call diagnostics_game_stage_login
    popfd
    popad

    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_login.return_addr]
  }
}

// CGameServer::onPreProcess Begin - server / shard picker (no stage emit)
extern "C" void __declspec(naked) hook_game_server_select() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_server_begin
    popfd
    popad

    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_server_select.return_addr]
  }
}

// CGameServer::onPreProcess End - shard selection screen ready
extern "C" void __declspec(naked) hook_game_main_menu() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_shard_select
    popfd
    popad

        // Original: mov ecx, offset aCGameServerOnPreProcessEnd (0x16B7288)
    mov ecx, 0x16B7288
    jmp [g_target_game_main_menu.return_addr]
  }
}

// Real post-server-select screens. Every target's onPreProcess starts with the
// same 6-byte prologue 55 8B EC 83 E4 F8 (push ebp; mov ebp,esp; and esp,-8);
// resume = target+6. Prologues verified in IDA (GAME 13337) 2026-05-29.
extern "C" void __declspec(naked) hook_game_lobby() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_lobby
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_lobby.return_addr]
  }
}

extern "C" void __declspec(naked) hook_game_room_list() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_room_list
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_room_list.return_addr]
  }
}

extern "C" void __declspec(naked) hook_game_party_room() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_party_room
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_party_room.return_addr]
  }
}

extern "C" void __declspec(naked) hook_game_room() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_room
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_room.return_addr]
  }
}

extern "C" void __declspec(naked) hook_game_char_select() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_char_select
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_char_select.return_addr]
  }
}

extern "C" void __declspec(naked) hook_game_map_loading() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_map_loading
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_map_loading.return_addr]
  }
}

extern "C" void __declspec(naked) hook_game_in_game() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_stage_in_game
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_game_in_game.return_addr]
  }
}

HookStub g_target_game_intro = {
    0x42A010,
    (uint32_t)(uintptr_t)hook_game_intro,
    "hook_game_intro",
    {0},
    false,
    0x42A016,
};

HookStub g_target_game_login = {
    0x42B280,
    (uint32_t)(uintptr_t)hook_game_login,
    "hook_game_login",
    {0},
    false,
    0x42B286,
};

HookStub g_target_game_server_select = {
    0x4345B0,
    (uint32_t)(uintptr_t)hook_game_server_select,
    "hook_game_server_select",
    {0},
    false,
    0x4345B6,
};

HookStub g_target_game_main_menu = {
    0x4347CC,
    (uint32_t)(uintptr_t)hook_game_main_menu,
    "hook_game_main_menu",
    {0},
    false,
    0x4347D1,
};

HookStub g_target_game_lobby = {
    0x42BD50,
    (uint32_t)(uintptr_t)hook_game_lobby,
    "hook_game_lobby",
    {0},
    false,
    0x42BD56,
};

HookStub g_target_game_room_list = {
    0x4362E0,
    (uint32_t)(uintptr_t)hook_game_room_list,
    "hook_game_room_list",
    {0},
    false,
    0x4362E6,
};

HookStub g_target_game_party_room = {
    0x42F690,
    (uint32_t)(uintptr_t)hook_game_party_room,
    "hook_game_party_room",
    {0},
    false,
    0x42F696,
};

HookStub g_target_game_room = {
    0x439B00, (uint32_t)(uintptr_t)hook_game_room, "hook_game_room", {0}, false,
    0x439B06,
};

HookStub g_target_game_char_select = {
    0x4F2DB0,
    (uint32_t)(uintptr_t)hook_game_char_select,
    "hook_game_char_select",
    {0},
    false,
    0x4F2DB6,
};

HookStub g_target_game_map_loading = {
    0x4806E0,
    (uint32_t)(uintptr_t)hook_game_map_loading,
    "hook_game_map_loading",
    {0},
    false,
    0x4806E6,
};

HookStub g_target_game_in_game = {
    0x47F610,
    (uint32_t)(uintptr_t)hook_game_in_game,
    "hook_game_in_game",
    {0},
    false,
    0x47F616,
};
