#include "thegame/paths.hpp"

#include <windows.h>

namespace thegame {

std::string game_directory() {
  char path[MAX_PATH];
  const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (!len || len >= MAX_PATH)
    return {};
  std::string exe(path, len);
  const size_t slash = exe.find_last_of("\\/");
  if (slash == std::string::npos)
    return {};
  return exe.substr(0, slash);
}

static std::string join_game_path(const char *filename) {
  const std::string dir = game_directory();
  if (dir.empty())
    return filename;
  return dir + "\\" + filename;
}

std::string main_log_path() { return join_game_path(kLogsTxt); }

std::string net_log_path() { return join_game_path(kNetlogsTxt); }

std::string proud_log_path() { return join_game_path(kProudlogsTxt); }

} // namespace thegame
