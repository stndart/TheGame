#pragma once

namespace Rmi {

// Autonomous custom-match navigation (env THEGAME_NAV_AUTO or TheGame.nav_auto).
// Values: "create_room", "full". Runs on the main thread from game_state hooks.
void NavLogStartup();

void NavOnStage(const char *phase);

// Retry pending UI transitions while transition lock clears (call every frame).
void NavPump(const char *phase);

} // namespace Rmi
