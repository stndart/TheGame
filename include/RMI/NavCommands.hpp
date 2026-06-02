#pragma once

namespace Rmi {

enum class NavCmd : unsigned char {
  None = 0,
  GotoLobby,
};

void NavEnqueueCommand(NavCmd cmd);

bool NavDequeueCommand(NavCmd *out);

bool NavHasQueuedCommands();

const char *NavCommandList();

} // namespace Rmi
