#include "RMI/Nav.hpp"

#include "RMI/Inject.hpp"
#include "diagnostics/handlers.hpp"

#include <cstdint>
#include <cstring>
#include <cwchar>
#include <windows.h>

namespace {

constexpr int kSceneServer = 2;
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

void *floor_singleton() {
  return reinterpret_cast<void *>(game_va(0x1C1ABB0));
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

// sub_A0B290: char __userpurge(proxy@<eax>, msg, len) — proxy dword_1C1ABB0 in EAX.
bool send_floor_rmi(const void *msg, int len) {
  void *proxy = floor_singleton();
  char ok = 0;
  const auto fn_va = game_va(0xA0B290);
  __asm {
    mov eax, proxy
    push len
    push msg
    call fn_va
    add esp, 8
    mov ok, al
  }
  return ok != 0;
}

void build_word_payload(unsigned char out[2], std::uint16_t floor_id) {
  out[0] = static_cast<unsigned char>(floor_id & 0xFF);
  out[1] = static_cast<unsigned char>((floor_id >> 8) & 0xFF);
}

void put_u32(unsigned char *p, std::size_t off, std::uint32_t v) {
  p[off + 0] = static_cast<unsigned char>(v & 0xFF);
  p[off + 1] = static_cast<unsigned char>((v >> 8) & 0xFF);
  p[off + 2] = static_cast<unsigned char>((v >> 16) & 0xFF);
  p[off + 3] = static_cast<unsigned char>((v >> 24) & 0xFF);
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

const char *nav_action() {
  static int cached = -1;
  static char buf[32];
  if (cached < 0) {
    const DWORD n =
        GetEnvironmentVariableA("THEGAME_NAV_ACTION", buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) {
      cached = 1;
    } else {
      buf[0] = '\0';
      cached = 0;
    }
  }
  return buf[0] ? buf : nullptr;
}

bool nav_is_create_mode() {
  const char *mode = nav_auto_mode();
  if (!mode)
    return false;
  return std::strcmp(mode, "create_room") == 0 || std::strcmp(mode, "full") == 0;
}

bool nav_is_exit_only_mode() {
  const char *mode = nav_auto_mode();
  return mode && std::strcmp(mode, "exit_lobby") == 0;
}

bool nav_enabled() {
  if (nav_action())
    return true;
  const char *mode = nav_auto_mode();
  if (!mode)
    return false;
  return nav_is_create_mode() || nav_is_exit_only_mode();
}

volatile LONG g_nav_logged = 0;
volatile LONG g_want_lobby = 0;
volatile LONG g_want_room_list = 0;
volatile LONG g_want_create_room = 0;
volatile LONG g_want_exit_server = 0;
volatile LONG g_did_server_ready = 0;
volatile LONG g_did_create_send = 0;
volatile LONG g_action_done = 0;

bool want_flag(volatile LONG *flag) {
  return InterlockedCompareExchange(flag, 0, 0) != 0;
}

void log_nav_enabled_once() {
  if (InterlockedCompareExchange(&g_nav_logged, 1, 0) != 0)
    return;
  const char *mode = nav_auto_mode();
  const char *action = nav_action();
  char line[128];
  wsprintfA(line, "nav: enabled mode=%s action=%s", mode ? mode : "-",
            action ? action : "-");
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

void send_server_leave() {
  unsigned char word[2];
  log_nav("c2s 0x3F0C server leave");
  build_word_payload(word, 0x3AD1);
  send_proxy_rmi(0x3F0C, word, 2);
  // Human exit also sends floor 0x3ACD len 6 (run 179). Direct sub_A0B290 call
  // from the DLL still faults — wire-only path until the wrapper is RE'd.
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

void send_leave_room_rmi(int param) {
  unsigned char body[6] = {};
  build_word_payload(body, 0x3AA3);
  put_u32(body, 2, static_cast<std::uint32_t>(param));
  log_nav("c2s 0x3F45 leave room (6B)");
  send_proxy_rmi(0x3F45, body, 6);
}

void send_quick_match_rmi() {
  unsigned char body[3] = {};
  build_word_payload(body, 0x3AAF);
  body[2] = 0;
  log_nav("c2s 0x3EE4 quick match (3B)");
  send_proxy_rmi(0x3EE4, body, 3);
}

void send_global_chat_rmi(const wchar_t *text) {
  unsigned char body[36] = {};
  build_word_payload(body, 0x3AD2);
  std::size_t n = 0;
  if (text) {
    while (text[n] && n < 12)
      ++n;
  }
  put_u32(body, 4, static_cast<std::uint32_t>(n));
  if (n)
    std::memcpy(body + 8, text, n * sizeof(wchar_t));
  log_nav("c2s floor 0x3AD2 global chat");
  send_floor_rmi(body, 36);
}

bool action_is(const char *name) {
  const char *action = nav_action();
  return action && std::strcmp(action, name) == 0;
}

bool action_blocks_create_forward() {
  return action_is("chat_ping") || action_is("quick_match");
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

bool try_exit_to_server() {
  const int scene = current_scene();
  if (scene == kSceneRoom) {
    send_leave_room_rmi(0);
    // Leave RES is pumped on this frame in PumpRoom (after NavPump).
    return current_scene() != kSceneRoom;
  }
  if (scene == kSceneRoomList || scene == kSceneLobby) {
    if (!request_scene(kSceneServer))
      return false;
    send_server_leave();
    return current_scene() == kSceneServer;
  }
  if (scene == kSceneServer)
    return true;
  char msg[64];
  wsprintfA(msg, "exit_to_server skipped (scene=%d)", scene);
  log_nav(msg);
  return false;
}

void run_one_shot_action() {
  if (InterlockedCompareExchange(&g_action_done, 1, 0) != 0)
    return;

  if (action_is("chat_ping")) {
    if (current_scene() == kSceneLobby) {
      send_global_chat_rmi(L"nav_ping");
      return;
    }
    InterlockedExchange(&g_action_done, 0);
    return;
  }

  if (action_is("quick_match")) {
    if (current_scene() == kSceneLobby) {
      send_quick_match_rmi();
      return;
    }
    InterlockedExchange(&g_action_done, 0);
    return;
  }

  if (action_is("leave_room")) {
    if (current_scene() == kSceneRoom) {
      send_leave_room_rmi(0);
      return;
    }
    InterlockedExchange(&g_action_done, 0);
    return;
  }

  InterlockedExchange(&g_action_done, 0);
}

void nav_on_server_ready() {
  if (InterlockedCompareExchange(&g_did_server_ready, 1, 0) != 0)
    return;
  if (nav_is_create_mode() || nav_is_exit_only_mode() || action_is("exit_lobby")) {
    send_server_ready_pair();
    InterlockedExchange(&g_want_lobby, 1);
  }
}

void nav_on_lobby() {
  if (action_is("exit_lobby") || nav_is_exit_only_mode())
    InterlockedExchange(&g_want_exit_server, 1);
  else if (nav_is_create_mode() && !action_blocks_create_forward())
    InterlockedExchange(&g_want_room_list, 1);
}

void nav_on_room_list() {
  if (action_is("exit_lobby") || nav_is_exit_only_mode())
    InterlockedExchange(&g_want_exit_server, 1);
  else if (nav_is_create_mode()) {
    InterlockedExchange(&g_want_create_room, 1);
    if (current_scene() == kSceneRoomList)
      send_room_list_refresh();
  }
}

void nav_on_room() {
  if (action_is("exit_lobby"))
    InterlockedExchange(&g_want_exit_server, 1);
}

void pump_one_shot_actions() {
  if (!nav_action() || action_is("exit_lobby"))
    return;
  if (transition_locked())
    return;
  if (action_is("chat_ping") || action_is("quick_match")) {
    if (current_scene() != kSceneLobby)
      return;
    run_one_shot_action();
    return;
  }
  if (action_is("leave_room")) {
    if (current_scene() != kSceneRoom)
      return;
    run_one_shot_action();
  }
}

} // namespace

void Rmi::NavLogStartup() {
  const char *mode = nav_auto_mode();
  const char *action = nav_action();
  if (mode || action) {
    char line[128];
    wsprintfA(line, "nav: startup enabled mode=%s action=%s",
              mode ? mode : "-", action ? action : "-");
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
    return;
  }

  if (std::strcmp(phase, "room") == 0) {
    nav_on_room();
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
      if (nav_is_create_mode() && !action_blocks_create_forward())
        InterlockedExchange(&g_want_room_list, 1);
      else if (nav_is_exit_only_mode() || action_is("exit_lobby"))
        InterlockedExchange(&g_want_exit_server, 1);
    }
  }

  if (want_flag(&g_want_room_list) && nav_is_create_mode()) {
    if (try_goto_room_list()) {
      InterlockedExchange(&g_want_room_list, 0);
      InterlockedExchange(&g_want_create_room, 1);
    }
  }

  if (want_flag(&g_want_create_room) && nav_is_create_mode() &&
      current_scene() == kSceneRoomList) {
    try_create_room();
    InterlockedExchange(&g_want_create_room, 0);
  }

  if (want_flag(&g_want_exit_server)) {
    if (try_exit_to_server()) {
      InterlockedExchange(&g_want_exit_server, 0);
      log_nav("exit_to_server done");
    }
  }

  pump_one_shot_actions();
}
