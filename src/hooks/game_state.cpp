#include "diagnostics/handlers.hpp"
#include "ProudNet/RmiInject.hpp"
#include "target_hooks.h"

extern "C" void __cdecl diagnostics_game_state_intro() {
  Diagnostics::emit_game_state("intro");
}

extern "C" void __cdecl diagnostics_game_state_login() {
  Diagnostics::emit_game_state("login");
}

extern "C" void __cdecl diagnostics_game_state_shard_choice() {
  Diagnostics::emit_game_state("shard_choice");
}

// NOTE: this fires at CGameServer::onPreProcess END (0x4347CC) - the server /
// shard picker finished its onEnter setup. It is NOT the real Main Menu (which
// needs human interaction to reach). Emitted as "server_ready" to stop the old
// deceptive "main_menu" label. Real screens are staged below (lobby, room, ...).
extern "C" void __cdecl diagnostics_game_state_main_menu() {
  Diagnostics::emit_game_state("server_ready");
}

extern "C" void __cdecl diagnostics_game_state_lobby() {
  Diagnostics::emit_game_state("lobby");
}

extern "C" void __cdecl diagnostics_game_state_room_list() {
  Diagnostics::emit_game_state("room_list");
  // Main-thread pump: run a pending Create-Room transition RES here (the frame
  // thread) instead of the ProudNet worker thread.
  Proud::RmiInject::PumpLobby();
}

extern "C" void __cdecl diagnostics_game_state_party_room() {
  Diagnostics::emit_game_state("party_room");
}

extern "C" void __cdecl diagnostics_game_state_room() {
  Diagnostics::emit_game_state("room");
  // Main-thread pump, BEFORE the original CGameRoom::onPreProcess binds room
  // data: populate the room (first frame), then honour a pending Ready->start.
  Proud::RmiInject::PumpRoom();
}

extern "C" void __cdecl diagnostics_game_state_char_select() {
  Diagnostics::emit_game_state("char_select");
}

extern "C" void __cdecl diagnostics_game_state_map_loading() {
  Diagnostics::emit_game_state("map_loading");
}

extern "C" void __cdecl diagnostics_game_state_in_game() {
  Diagnostics::emit_game_state("in_game");
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
    call diagnostics_game_state_login
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

// Real post-server-select screens. Every target's onPreProcess starts with the
// same 6-byte prologue 55 8B EC 83 E4 F8 (push ebp; mov ebp,esp; and esp,-8);
// resume = target+6. Prologues verified in IDA (GAME 13337) 2026-05-29.
extern "C" void __declspec(naked) hook_game_lobby() {
  __asm {
    pushad
    pushfd
    call diagnostics_game_state_lobby
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
    call diagnostics_game_state_room_list
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
    call diagnostics_game_state_party_room
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
    call diagnostics_game_state_room
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
    call diagnostics_game_state_char_select
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
    call diagnostics_game_state_map_loading
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
    call diagnostics_game_state_in_game
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
    0x42BD50, (uint32_t)(uintptr_t)hook_game_lobby, "hook_game_lobby",
    {0},      false,                                0x42BD56,
};

HookStub g_target_game_room_list = {
    0x4362E0, (uint32_t)(uintptr_t)hook_game_room_list, "hook_game_room_list",
    {0},      false,                                    0x4362E6,
};

HookStub g_target_game_party_room = {
    0x42F690, (uint32_t)(uintptr_t)hook_game_party_room, "hook_game_party_room",
    {0},      false,                                     0x42F696,
};

HookStub g_target_game_room = {
    0x439B00, (uint32_t)(uintptr_t)hook_game_room, "hook_game_room",
    {0},      false,                               0x439B06,
};

HookStub g_target_game_char_select = {
    0x4F2DB0, (uint32_t)(uintptr_t)hook_game_char_select, "hook_game_char_select",
    {0},      false,                                      0x4F2DB6,
};

HookStub g_target_game_map_loading = {
    0x4806E0, (uint32_t)(uintptr_t)hook_game_map_loading, "hook_game_map_loading",
    {0},      false,                                      0x4806E6,
};

HookStub g_target_game_in_game = {
    0x47F610, (uint32_t)(uintptr_t)hook_game_in_game, "hook_game_in_game",
    {0},      false,                                  0x47F616,
};
