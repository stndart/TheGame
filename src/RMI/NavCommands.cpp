#include "RMI/NavCommands.hpp"

#include "RMI/Nav.hpp"
#include "thegame/log.hpp"
#include <windows.h>

namespace {

constexpr int kQueueSize = 16;
Rmi::NavCmd g_queue[kQueueSize];
int g_head = 0;
int g_tail = 0;
CRITICAL_SECTION g_queue_lock;
bool g_lock_init = false;

void ensure_lock() {
  if (!g_lock_init) {
    InitializeCriticalSection(&g_queue_lock);
    g_lock_init = true;
  }
}

} // namespace

void Rmi::NavEnqueueCommand(NavCmd cmd) {
  if (cmd == NavCmd::None)
    return;
  ensure_lock();
  EnterCriticalSection(&g_queue_lock);
  if (g_tail - g_head < kQueueSize) {
    g_queue[g_tail++ % kQueueSize] = cmd;
    if (cmd == NavCmd::PassShardSelect)
      thegame::logf("[nav] enqueued nav_pass_shard_select (handler pipe)");
    NavSchedulePump();
  } else {
    thegame::logf("[nav] command queue full, dropped");
  }
  LeaveCriticalSection(&g_queue_lock);
}

bool Rmi::NavHasQueuedCommands() {
  ensure_lock();
  EnterCriticalSection(&g_queue_lock);
  const bool has = g_head < g_tail;
  LeaveCriticalSection(&g_queue_lock);
  return has;
}

bool Rmi::NavDequeueCommand(NavCmd *out) {
  if (!out)
    return false;
  ensure_lock();
  EnterCriticalSection(&g_queue_lock);
  if (g_head >= g_tail) {
    LeaveCriticalSection(&g_queue_lock);
    return false;
  }
  *out = g_queue[g_head++ % kQueueSize];
  LeaveCriticalSection(&g_queue_lock);
  return true;
}

const char *Rmi::NavCommandList() {
  return "nav_pass_shard_select";
}
