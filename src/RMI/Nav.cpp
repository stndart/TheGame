#include "RMI/Nav.hpp"

#include "RMI/Inject.hpp"
#include "diagnostics/handlers.hpp"

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <windows.h>

namespace {

constexpr int kSceneLobby = 4;
constexpr int kSceneRoomList = 5;
constexpr int kSceneRoom = 9;

std::uintptr_t game_va(const std::uint32_t va) {
  const std::uintptr_t delta =
      reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<std::uintptr_t>(va) + delta;
}

template <typename T> T &game_global(const std::uint32_t va) {
  return *reinterpret_cast<T *>(game_va(va));
}

void *state_machine() {
  return reinterpret_cast<void *>(game_va(0x1C155C0));
}

void *proxy_singleton() {
  return reinterpret_cast<void *>(game_va(0x1C1ABA0));
}

bool transition_locked() {
  return game_global<std::uint8_t>(0x1C1E409) != 0;
}

int current_scene() { return game_global<int>(0x1C15644); }

void log_nav(const char *msg) {
  char line[128];
  wsprintfA(line, "nav: %s (tid=%lu)", msg, GetCurrentThreadId());
  Diagnostics::emit_game_log(line);
}

using RequestStateFn = char(__thiscall *)(void *, int);
using OnClickRoomListFn = int(__stdcall *)(void *);
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

bool invoke_on_click_room_list() {
  if (transition_locked()) {
    log_nav("onClickRoomList blocked (transition lock)");
    return false;
  }
  if (current_scene() == kSceneRoomList)
    return true;
  if (current_scene() != kSceneLobby) {
    char msg[64];
    wsprintfA(msg, "onClickRoomList skipped (scene=%d)", current_scene());
    log_nav(msg);
    return false;
  }
  log_nav("onClickRoomList 0x42CAE0");
  const auto fn = reinterpret_cast<OnClickRoomListFn>(game_va(0x42CAE0));
  return fn(nullptr) != 0;
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

void build_create_room_req(unsigned char out[98], const wchar_t *name) {
  std::memset(out, 0, 98);
  build_word_payload(out, 0x3AA0);
  if (name) {
    std::size_t n = 0;
    while (name[n] && n < 26)
      ++n;
    std::memcpy(out + 2, name, n * sizeof(wchar_t));
  }
}

bool read_nav_sidecar(char *buf, std::size_t buf_size) {
  wchar_t exe_path[MAX_PATH] = {};
  if (!GetModuleFileNameW(nullptr, exe_path, MAX_PATH))
    return false;
  wchar_t *slash = wcsrchr(exe_path, L'\\');
  if (!slash)
    return false;
  const wchar_t suffix[] = L"TheGame.nav_auto";
  if (static_cast<std::size_t>(slash + 1 - exe_path) + wcslen(suffix) >=
      MAX_PATH)
    return false;
  wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - exe_path), suffix);
  HANDLE h = CreateFileW(exe_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  char tmp[32] = {};
  DWORD read = 0;
  const BOOL ok =
      ReadFile(h, tmp, static_cast<DWORD>(sizeof(tmp) - 1), &read, nullptr);
  CloseHandle(h);
  if (!ok || read == 0)
    return false;
  tmp[read] = '\0';
  for (DWORD i = 0; i < read; ++i) {
    if (tmp[i] == '\r' || tmp[i] == '\n')
      tmp[i] = '\0';
  }
  if (!tmp[0])
    return false;
  strncpy_s(buf, buf_size, tmp, _TRUNCATE);
  return true;
}

const char *nav_auto_mode() {
  static int cached = -1;
  static char buf[32];
  if (cached < 0) {
    const DWORD n =
        GetEnvironmentVariableA("THEGAME_NAV_AUTO", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
      cached = 1;
    } else if (read_nav_sidecar(buf, sizeof(buf))) {
      cached = 1;
    } else {
      buf[0] = '\0';
      cached = 0;
    }
  }
  return buf[0] ? buf : nullptr;
}

bool nav_enabled() {
  const char *mode = nav_auto_mode();
  if (!mode)
    return false;
  return std::strcmp(mode, "create_room") == 0 || std::strcmp(mode, "full") == 0;
}

volatile LONG g_nav_logged = 0;
volatile LONG g_want_lobby = 0;
volatile LONG g_want_room_list = 0;
volatile LONG g_want_create_room = 0;
volatile LONG g_did_server_ready = 0;
volatile LONG g_did_create_send = 0;

bool want_flag(volatile LONG *flag) {
  return InterlockedCompareExchange(flag, 0, 0) != 0;
}

void log_nav_enabled_once() {
  if (InterlockedCompareExchange(&g_nav_logged, 1, 0) != 0)
    return;
  const char *mode = nav_auto_mode();
  char line[96];
  wsprintfA(line, "nav: enabled mode=%s", mode ? mode : "?");
  Diagnostics::emit_game_log(line);
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

void send_room_list_refresh() {
  unsigned char word[2];
  log_nav("c2s 0x3F2F room list");
  build_word_payload(word, 0x3A9F);
  send_proxy_rmi(0x3F2F, word, 2);
}

void send_create_room_rmi() {
  unsigned char body[98];
  log_nav("c2s 0x3F30 create room (98B body)");
  build_create_room_req(body, L"re_room");
  send_proxy_rmi(0x3F30, body, 98);
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

bool try_goto_room_list() {
  if (current_scene() == kSceneRoomList)
    return true;
  if (current_scene() == kSceneLobby) {
    if (invoke_on_click_room_list())
      return current_scene() == kSceneRoomList;
    return request_scene(kSceneRoomList);
  }
  return request_scene(kSceneRoomList);
}

bool try_create_room() {
  if (InterlockedCompareExchange(&g_did_create_send, 1, 0) != 0)
    return true;
  send_create_room_rmi();
  return true;
}

void nav_on_server_ready() {
  if (InterlockedCompareExchange(&g_did_server_ready, 1, 0) != 0)
    return;
  send_server_ready_pair();
  InterlockedExchange(&g_want_lobby, 1);
}

void nav_on_lobby() {
  InterlockedExchange(&g_want_room_list, 1);
}

void nav_on_room_list() {
  InterlockedExchange(&g_want_create_room, 1);
  if (current_scene() == kSceneRoomList)
    send_room_list_refresh();
}

} // namespace

void Rmi::NavLogStartup() {
  const char *mode = nav_auto_mode();
  if (mode) {
    char line[96];
    wsprintfA(line, "nav: startup enabled mode=%s", mode);
    Diagnostics::emit_game_log(line);
  } else {
    Diagnostics::emit_game_log("nav: startup disabled (no env/sidecar)");
  }
}

void Rmi::NavOnStage(const char *phase) {
  if (!phase || !nav_enabled())
    return;

  log_nav_enabled_once();

  if (std::strcmp(phase, "server_ready") == 0) {
    nav_on_server_ready();
    NavPump(phase);
    return;
  }

  if (std::strcmp(phase, "lobby") == 0) {
    nav_on_lobby();
    NavPump(phase);
    return;
  }

  if (std::strcmp(phase, "room_list") == 0) {
    nav_on_room_list();
    NavPump(phase);
  }
}

void Rmi::NavPump(const char *phase) {
  (void)phase;
  if (!nav_enabled())
    return;

  if (want_flag(&g_want_lobby)) {
    if (try_goto_lobby()) {
      InterlockedExchange(&g_want_lobby, 0);
      InterlockedExchange(&g_want_room_list, 1);
    }
  }

  if (want_flag(&g_want_room_list)) {
    if (try_goto_room_list()) {
      InterlockedExchange(&g_want_room_list, 0);
      InterlockedExchange(&g_want_create_room, 1);
    }
  }

  if (want_flag(&g_want_create_room) && current_scene() == kSceneRoomList) {
    try_create_room();
    InterlockedExchange(&g_want_create_room, 0);
  }
}
