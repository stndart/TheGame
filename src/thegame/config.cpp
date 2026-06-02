#include "thegame/config.hpp"

#include <windows.h>

namespace thegame {

Cfg cfg = {};

namespace {

bool env_truthy_value(const char *value) {
  if (!value || !value[0])
    return false;
  if (value[0] == '0')
    return false;
  if (_stricmp(value, "false") == 0 || _stricmp(value, "off") == 0 ||
      _stricmp(value, "no") == 0)
    return false;
  return true;
}

bool merge_flag(const char *name, int compile_default) {
  char buf[64];
  const DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
  if (n > 0)
    return env_truthy_value(buf);
  return compile_default != 0;
}

std::string merge_string(const char *name, const char *compile_default) {
  char buf[64];
  const DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
  if (n > 0)
    return buf;
  return compile_default;
}
} // namespace

void init_config() {
  // compile/env flags
  cfg.no_console = merge_flag("THEGAME_NO_CONSOLE", THEGAME_NO_CONSOLE);
  cfg.pipes = merge_flag("THEGAME_PIPES", THEGAME_PIPES);
  cfg.disable_hooks = merge_flag("DISABLE_HOOKS", DISABLE_HOOKS);
  cfg.disable_syshooks = merge_flag("DISABLE_SYSHOOKS", DISABLE_SYSHOOKS);
  cfg.disable_entrypoint_hook =
      merge_flag("DISABLE_ENTRYPOINT_HOOK", DISABLE_ENTRYPOINT_HOOK);
  cfg.disable_veh = merge_flag("DISABLE_VEH", DISABLE_VEH);
  cfg.disable_park_thread =
      merge_flag("DISABLE_PARK_THREAD", DISABLE_PARK_THREAD);
  cfg.disable_int = merge_flag("DISABLE_INT", DISABLE_INT);
  cfg.minidump_enabled = merge_flag("MINIDUMP_ENABLED", MINIDUMP_ENABLED);
  cfg.disable_autonav = merge_flag("DISABLE_AUTONAV", DISABLE_AUTONAV);
  cfg.no_network_logs = merge_flag("NO_NETWORK_LOGS", NO_NETWORK_LOGS);
  cfg.silent_keepalive = merge_flag("SILENT_KEEPALIVE", SILENT_KEEPALIVE);
  cfg.silent_network = merge_flag("SILENT_NETWORK", SILENT_NETWORK);
  cfg.no_proud_logs = merge_flag("NO_PROUD_LOGS", NO_PROUD_LOGS);
  cfg.silent_proud = merge_flag("SILENT_PROUD", SILENT_PROUD);

  // env only
  cfg.server_override = merge_flag("THEGAME_SERVER_OVERRIDE", 0);
  cfg.server_ip = merge_string("THEGAME_SERVER_IP", "127.0.0.1");
}

} // namespace thegame
