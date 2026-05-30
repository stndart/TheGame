#pragma once

namespace Rmi {

// Autonomous custom-match navigation.
//   THEGAME_NAV_AUTO or TheGame.nav_auto — "create_room", "full", "exit_lobby"
//   THEGAME_NAV_ACTION — one-shot: "exit_lobby", "chat_ping", "quick_match",
//                        "leave_room" (combinable with create_room / full)
// Runs on the main thread from game_state hooks.
void NavLogStartup();

void NavOnStage(const char *phase);

// Retry pending UI transitions while transition lock clears (call every frame).
void NavPump(const char *phase);

} // namespace Rmi
