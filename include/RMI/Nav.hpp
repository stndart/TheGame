#pragma once

namespace Rmi {

// Lobby navigation via ctl handler pipe (nav_goto_lobby). Runs on main thread.
void NavStartup();
void NavTeardown();

void NavOnStage(const char *phase);

void NavPump(const char *phase);

void NavDrainCommands();

} // namespace Rmi
