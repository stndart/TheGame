#include "RMI/Nav.hpp"

#include <cstdint>
#include <windows.h>

#include "RMI/NavCommands.hpp"
#include "thegame/log.hpp"

using thegame::logf;

namespace {

constexpr int kSceneLobby = 4;

std::uintptr_t game_va(const std::uint32_t va) {
  const std::uintptr_t delta =
      reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<std::uintptr_t>(va) + delta;
}

template <typename T> T &game_global(const std::uint32_t va) {
  return *reinterpret_cast<T *>(game_va(va));
}

void *proxy_singleton() { return reinterpret_cast<void *>(game_va(0x1C1ABA0)); }

bool transition_locked() { return game_global<std::uint8_t>(0x1C1E409) != 0; }

int current_scene() { return game_global<int>(0x1C15644); }

void log_nav(const char *msg) {
  char line[128];
  wsprintfA(line, "[nav] %s (tid=%lu)", msg, GetCurrentThreadId());
  logf(line);
}

using SendProxyFn = int(__thiscall *)(void *, void *, int, short);

bool send_proxy_rmi(unsigned id, const void *msg, int len) {
  const auto fn = reinterpret_cast<SendProxyFn>(game_va(0x65AEA0));
  return fn(proxy_singleton(), const_cast<void *>(msg), len,
            static_cast<short>(id & 0xFFFFu)) != 0;
}

void build_word_payload(unsigned char out[2], std::uint16_t floor_id) {
  out[0] = static_cast<unsigned char>(floor_id & 0xFF);
  out[1] = static_cast<unsigned char>((floor_id >> 8) & 0xFF);
}

void send_lobby_enter_notify() {
  unsigned char word[2];
  log_nav("c2s 0x3F40 lobby enter");
  build_word_payload(word, 0x3ACE);
  send_proxy_rmi(0x3F40, word, 2);
}

volatile LONG g_goto_lobby_pending = 0;
volatile LONG g_did_lobby_notify_send = 0;
volatile LONG g_wakeup_thread_running = 0;

volatile DWORD g_main_tid = 0;
HHOOK g_getmsg_hook = nullptr;
volatile LONG g_getmsg_hook_logged = 0;

void note_main_thread() {
  const DWORD tid = GetCurrentThreadId();
  InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&g_main_tid),
                             static_cast<LONG>(tid), 0);
}

LRESULT CALLBACK nav_getmsg_proc(int code, WPARAM wParam, LPARAM lParam) {
  (void)wParam;
  (void)lParam;
  if (code == HC_ACTION &&
      InterlockedCompareExchange(&g_goto_lobby_pending, 0, 0) != 0)
    Rmi::NavPump("getmsg");
  else if (code == HC_ACTION && Rmi::NavHasQueuedCommands())
    Rmi::NavPump("getmsg");
  return CallNextHookEx(g_getmsg_hook, code, wParam, lParam);
}

void ensure_getmsg_hook() {
  if (g_getmsg_hook || g_main_tid == 0)
    return;
  HMODULE mod = GetModuleHandleA("TheGame.dll");
  if (!mod)
    return;
  g_getmsg_hook =
      SetWindowsHookExA(WH_GETMESSAGE, nav_getmsg_proc, mod, g_main_tid);
  if (g_getmsg_hook &&
      InterlockedCompareExchange(&g_getmsg_hook_logged, 1, 0) == 0)
    log_nav("WH_GETMESSAGE hook installed");
}

void wake_main_thread() {
  if (g_main_tid == 0)
    return;
  PostThreadMessageA(g_main_tid, WM_NULL, 0, 0);
}

DWORD WINAPI nav_wakeup_thread(LPVOID) {
  for (int i = 0; i < 120; ++i) {
    if (InterlockedCompareExchange(&g_goto_lobby_pending, 0, 0) == 0)
      break;
    wake_main_thread();
    Sleep(500);
  }
  InterlockedExchange(&g_wakeup_thread_running, 0);
  return 0;
}

void start_nav_wakeup_thread() {
  if (InterlockedCompareExchange(&g_wakeup_thread_running, 1, 0) != 0)
    return;
  HANDLE th = CreateThread(nullptr, 0, nav_wakeup_thread, nullptr, 0, nullptr);
  if (th)
    CloseHandle(th);
  else
    InterlockedExchange(&g_wakeup_thread_running, 0);
}

void pump_goto_lobby() {
  static volatile LONG s_logged_lock = 0;

  if (InterlockedCompareExchange(&g_goto_lobby_pending, 0, 0) == 0)
    return;

  if (current_scene() == kSceneLobby) {
    InterlockedExchange(&g_goto_lobby_pending, 0);
    InterlockedExchange(&g_did_lobby_notify_send, 0);
    InterlockedExchange(&s_logged_lock, 0);
    log_nav("goto_lobby done");
    return;
  }

  if (transition_locked()) {
    if (InterlockedCompareExchange(&s_logged_lock, 1, 0) == 0)
      log_nav("goto_lobby blocked (transition lock)");
    return;
  }
  InterlockedExchange(&s_logged_lock, 0);

  // shard_select already sends server-enter 0x3F0C + 0x3E99; lobby step is 0x3F40.
  if (InterlockedCompareExchange(&g_did_lobby_notify_send, 1, 0) == 0)
    send_lobby_enter_notify();
}

} // namespace

void Rmi::NavDrainCommands() {
  NavCmd cmd = NavCmd::None;
  while (NavDequeueCommand(&cmd)) {
    if (cmd == NavCmd::GotoLobby) {
      log_nav("command nav_goto_lobby");
      InterlockedExchange(&g_did_lobby_notify_send, 0);
      InterlockedExchange(&g_goto_lobby_pending, 1);
      start_nav_wakeup_thread();
    }
  }
}

void Rmi::NavSchedulePump() {
  ensure_getmsg_hook();
  wake_main_thread();
}

void Rmi::NavStartup() { logf("[nav] command-driven (handler pipe)"); }

void Rmi::NavTeardown() {
  if (g_getmsg_hook) {
    UnhookWindowsHookEx(g_getmsg_hook);
    g_getmsg_hook = nullptr;
  }
  InterlockedExchange(reinterpret_cast<volatile LONG *>(&g_main_tid), 0);
  InterlockedExchange(&g_getmsg_hook_logged, 0);
  logf("[nav] teardown");
}

void Rmi::NavPump(const char *phase) {
  (void)phase;
  note_main_thread();
  ensure_getmsg_hook();
  NavDrainCommands();
  pump_goto_lobby();
}
