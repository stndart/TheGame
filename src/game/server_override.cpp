#include "game/server_override.hpp"

#include <cstring>
#include <fstream>
#include <ws2tcpip.h>
#include <windows.h>

namespace {

bool is_hardcoded_emu_host(const char *host) {
  if (!host)
    return false;
  return strcmp(host, ServerOverride::kEntryHost) == 0 ||
         strcmp(host, ServerOverride::kLobbyHost) == 0;
}

unsigned long production_ipv4(const char *dotted) {
  return inet_addr(dotted);
}

std::string trim_line(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
    s.pop_back();
  size_t start = 0;
  while (start < s.size() && s[start] == ' ')
    ++start;
  return s.substr(start);
}

std::string game_exe_directory() {
  char path[MAX_PATH];
  DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (!len || len >= MAX_PATH)
    return {};
  std::string exe(path, len);
  const size_t slash = exe.find_last_of("\\/");
  if (slash == std::string::npos)
    return {};
  return exe.substr(0, slash);
}

std::string read_override_file() {
  const std::string dir = game_exe_directory();
  if (dir.empty())
    return {};

  std::ifstream in(dir + "\\" + ServerOverride::kOverrideFileName);
  if (!in)
    return {};

  std::string line;
  std::getline(in, line);
  return trim_line(line);
}

std::string cached_override() {
  static bool loaded = false;
  static std::string value;
  if (!loaded) {
    loaded = true;
    value = read_override_file();
    const char *env = getenv("THEGAME_SERVER_IP");
    if (env && *env)
      value = trim_line(env);
  }
  return value;
}

} // namespace

namespace ServerOverride {

bool override_active() { return !cached_override().empty(); }

bool is_pn_port(const uint16_t port) {
  return port == kEntryPort || port == kGameLegPort || port == kProbePort;
}

bool is_production_ipv4(const unsigned long ipv4) {
  if (ipv4 == INADDR_NONE || ipv4 == INADDR_ANY)
    return false;
  return ipv4 == production_ipv4(kEntryHost) ||
         ipv4 == production_ipv4(kLobbyHost);
}

std::string remap_host(const char *host) {
  const std::string override_ip = cached_override();
  if (override_ip.empty() || !is_hardcoded_emu_host(host))
    return {};
  return override_ip;
}

unsigned long remap_ipv4(const unsigned long ipv4, const uint16_t port) {
  const std::string override_ip = cached_override();
  if (override_ip.empty() || !is_production_ipv4(ipv4))
    return 0;
  if (port != 0 && !is_pn_port(port))
    return 0;
  const unsigned long target = inet_addr(override_ip.c_str());
  if (target == INADDR_NONE)
    return 0;
  return target;
}

bool remap_sockaddr_in(sockaddr_in *in) {
  if (!in || in->sin_family != AF_INET)
    return false;
  const uint16_t port = ntohs(in->sin_port);
  const unsigned long mapped = remap_ipv4(in->sin_addr.s_addr, port);
  if (!mapped)
    return false;
  in->sin_addr.s_addr = mapped;
  return true;
}

} // namespace ServerOverride
