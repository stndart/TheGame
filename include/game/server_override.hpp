#pragma once

#include <cstdint>
#include <string>

struct sockaddr_in;

namespace ServerOverride {

// Path beside GAME.exe: TheGame.server_ip (written by ctl launch /
// launch_game.py).
constexpr const char *kOverrideFileName = "TheGame.server_ip";

// Hostnames from encrypted options.dat / redirects (see GITS-FA-emulation
// NETWORKING.md).
constexpr const char *kEntryHost = "137.184.201.52";
constexpr const char *kLobbyHost = "216.131.86.188";

// ProudNet legs (ENTRY / GAME / PROBE) - remap these ports in offline mode.
constexpr uint16_t kEntryPort = 7000;
constexpr uint16_t kGameLegPort = 27380;
constexpr uint16_t kProbePort = 20009;

// True when TheGame.server_ip / THEGAME_SERVER_IP is set (offline / local
// entry).
bool override_active();

bool is_pn_port(uint16_t port);

bool is_production_ipv4(unsigned long ipv4);

// Returns non-empty override IP when a remap should be applied for this host.
std::string remap_host(const char *host);

// IPv4 from inet_addr; returns 0 if no remap. Port optional (0 = any).
unsigned long remap_ipv4(unsigned long ipv4, uint16_t port = 0);

// Patch sockaddr_in in place; returns true when address changed.
bool remap_sockaddr_in(sockaddr_in *in);

} // namespace ServerOverride
