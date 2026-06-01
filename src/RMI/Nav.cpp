#include "RMI/Nav.hpp"

#include <cstdint>
#include <windows.h>

#include "RMI/Inject.hpp"
#include "RMI/NavCommands.hpp"
#include "thegame/log.hpp"

using thegame::logf;

namespace {

constexpr int kSceneServer = 2;
constexpr int kSceneLobby = 4;

std::uintptr_t game_va(const std::uint32_t va) {
  const std::uintptr_t delta =
      reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<std::uintptr_t>(va) + delta;
}

template <typename T> T &game_global(const std::uint32_t va) {
  return *reinterpret_cast<T *>(game_va(va));
}

void *state_machine() { return reinterpret_cast<void *>(game_va(0x1C155C0)); }

void *proxy_singleton() { return reinterpret_cast<void *>(game_va(0x1C1ABA0)); }

bool transition_locked() { return game_global<std::uint8_t>(0x1C1E409) != 0; }

int current_scene() { return game_global<int>(0x1C15644); }

void log_nav(const char *msg) {
  char line[128];
  wsprintfA(line, "nav: %s (tid=%lu)", msg, GetCurrentThreadId());
  logf(line);
}

using RequestStateFn = char(__thiscall *)(void *, int);
using SendProxyFn = int(__thiscall *)(void *, void *, int, short);

bool request_scene(int scene_id) {
  if (transition_locked()) {
    log_nav("RequestState blocked (transition lock)");
    return false;
  }
  if (current_scene() == scene_id)
    return true;
  char msg[64];
  wsprintfA(msg, "RequestState(%d) from scene %d", scene_id, current_scene());
  log_nav(msg);
  const auto fn = reinterpret_cast<RequestStateFn>(game_va(0x41F0D0));
  return fn(state_machine(), scene_id) != 0;
}

bool send_proxy_rmi(unsigned id, const void *msg, int len) {
  const auto fn = reinterpret_cast<SendProxyFn>(game_va(0x65AEA0));
  return fn(proxy_singleton(), const_cast<void *>(msg), len,
            static_cast<short>(id & 0xFFFFu)) != 0;
}

void build_word_payload(unsigned char out[2], std::uint16_t floor_id) {
  out[0] = static_cast<unsigned char>(floor_id & 0xFF);
  out[1] = static_cast<unsigned char>((floor_id >> 8) & 0xFF);
}

volatile LONG g_want_lobby = 0;
volatile LONG g_did_server_ready = 0;

bool want_flag(volatile LONG *flag) {
  return InterlockedCompareExchange(flag, 0, 0) != 0;
}

void send_server_ready_pair() {
  unsigned char word[2];
  log_nav("c2s 0x3F0C server enter");
  build_word_payload(word, 0x3AD1);
  send_proxy_rmi(0x3F0C, word, 2);
  log_nav("c2s 0x3E99 notify");
  build_word_payload(word, 0x3AD4);
  send_proxy_rmi(0x3E99, word, 2);
}

bool try_goto_lobby() {
  if (current_scene() == kSceneLobby)
    return true;
  log_nav("inject lobby-enter RES 0x3F41");
  Rmi::InjectLobbyEnterRes();
  if (current_scene() == kSceneLobby)
    return true;
  return request_scene(kSceneLobby);
}

void arm_goto_lobby() {
  if (InterlockedCompareExchange(&g_did_server_ready, 1, 0) == 0)
    send_server_ready_pair();
  InterlockedExchange(&g_want_lobby, 1);
}

} // namespace

void Rmi::NavDrainCommands() {
  NavCmd cmd = NavCmd::None;
  while (NavDequeueCommand(&cmd)) {
    if (cmd == NavCmd::GotoLobby) {
      log_nav("command nav_goto_lobby");
      arm_goto_lobby();
    }
  }
}

void Rmi::NavStartup() { logf("nav: command-driven (autonav disabled)"); }

void Rmi::NavTeardown() { logf("nav: teardown"); }

void Rmi::NavOnStage(const char *phase) { (void)phase; }

void Rmi::NavPump(const char *phase) {
  (void)phase;
  NavDrainCommands();

  if (!want_flag(&g_want_lobby))
    return;

  if (try_goto_lobby()) {
    InterlockedExchange(&g_want_lobby, 0);
    log_nav("goto_lobby done");
  }
}
