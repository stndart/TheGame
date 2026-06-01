#include "game/server_override.hpp"

#include <windows.h>
#include <ws2tcpip.h>

#include "thegame/config.hpp"

namespace ServerOverride {

bool is_pn_hostname(const char *hostname) {
  return strcmp(hostname, kEntryHost) == 0;
}

bool is_pn_address(unsigned long ipv4) {
  static unsigned long pn_address = inet_addr(kEntryHost);
  return ipv4 == pn_address;
}

bool is_pn_port(uint16_t port) {
  return port == kEntryPort || port == kGameLegPort || port == kProbePort;
}

unsigned long get_target_address() {
  static unsigned long target = inet_addr(thegame::cfg.server_ip.c_str());
  return target;
}

bool override_active() {
  if (!thegame::cfg.server_override)
    return false;
  
  if (thegame::cfg.server_ip.empty())
    return false;
  
  if (get_target_address() == INADDR_NONE)
    return false;

  return true;
}

std::string remap_host(const char *host) {
  if (!override_active())
    return host;

  if (is_pn_hostname(host))
    return thegame::cfg.server_ip;

  return host;
}

bool remap_sockaddr_in(sockaddr_in *in) {
  if (!override_active())
    return false;

  if (is_pn_address(in->sin_addr.s_addr) && is_pn_port(ntohs(in->sin_port))) {
    in->sin_addr.s_addr = get_target_address();
    return true;
  }
  return false;
}

} // namespace ServerOverride
