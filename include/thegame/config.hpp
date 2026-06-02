#pragma once

#include <string>

// Compile-time defaults (0/1). CMake may define overrides via
// target_compile_definitions.

#ifndef THEGAME_NO_CONSOLE
#define THEGAME_NO_CONSOLE 0
#endif

#ifndef THEGAME_PIPES
#define THEGAME_PIPES 1
#endif

#ifndef DISABLE_HOOKS
#define DISABLE_HOOKS 0
#endif

#ifndef DISABLE_SYSHOOKS
#define DISABLE_SYSHOOKS 0
#endif

#ifndef DISABLE_ENTRYPOINT_HOOK
#define DISABLE_ENTRYPOINT_HOOK 0
#endif

#ifndef DISABLE_VEH
#define DISABLE_VEH 0
#endif

#ifndef DISABLE_PARK_THREAD
#define DISABLE_PARK_THREAD 1
#endif

#ifndef DISABLE_INT
#define DISABLE_INT 1
#endif

#ifndef MINIDUMP_ENABLED
#define MINIDUMP_ENABLED 0
#endif

#ifndef NO_NETWORK_LOGS
#define NO_NETWORK_LOGS 0
#endif

#ifndef SILENT_KEEPALIVE
#define SILENT_KEEPALIVE 1
#endif

#ifndef SILENT_NETWORK
#define SILENT_NETWORK 0
#endif

#ifndef NO_PROUD_LOGS
#define NO_PROUD_LOGS 0
#endif

#ifndef SILENT_PROUD
#define SILENT_PROUD 0
#endif

#ifndef WS2_HOOKS
#define WS2_HOOKS 1
#endif

#ifndef DISABLE_GAME_STARTUP_TRASH
#define DISABLE_GAME_STARTUP_TRASH 1
#endif

#ifndef LOG_WCONVERT
#define LOG_WCONVERT 0
#endif

#ifndef ALLOC_LOG
#define ALLOC_LOG 0
#endif

#ifndef RESERVE_LOG
#define RESERVE_LOG 0
#endif

#ifndef CONCAT_LOG
#define CONCAT_LOG 0
#endif

#ifndef FORMAT_LOG
#define FORMAT_LOG 0
#endif

namespace thegame {

struct Cfg {
  // compile/env flags
  bool no_console;
  bool pipes;
  bool disable_hooks;
  bool disable_syshooks;
  bool disable_entrypoint_hook;
  bool disable_veh;
  bool disable_park_thread;
  bool disable_int;
  bool minidump_enabled;
  bool no_network_logs;
  bool silent_keepalive;
  bool silent_network;
  bool no_proud_logs;
  bool silent_proud;

  // env only
  bool server_override;
  std::string server_ip;
};

extern Cfg cfg;

void init_config();

} // namespace thegame
