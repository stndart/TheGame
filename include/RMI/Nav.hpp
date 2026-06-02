#pragma once

namespace Rmi {

// Lobby navigation via ctl handler pipe (nav_goto_lobby). Runs on main thread.
void NavStartup();
void NavTeardown();

void NavPump(const char *phase);

void NavDrainCommands();

// Post a main-thread pump (handler pipe enqueues off-thread).
void NavSchedulePump();

} // namespace Rmi
