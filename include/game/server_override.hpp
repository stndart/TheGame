#pragma once

#include <string>

namespace ServerOverride {

// Path beside GAME.exe: TheGame.server_ip (written by ctl launch / launch_game.py).
constexpr const char *kOverrideFileName = "TheGame.server_ip";

// Hostnames from encrypted options.dat / redirects (see GITS-FA-emulation NETWORKING.md).
constexpr const char *kEntryHost = "137.184.201.52";
constexpr const char *kLobbyHost = "216.131.86.188";

// True when TheGame.server_ip / THEGAME_SERVER_IP is set (offline / local entry).
bool override_active();

// Returns non-empty override IP when a remap should be applied for this host.
std::string remap_host(const char *host);

// IPv4 from inet_addr; returns 0 if no remap.
unsigned long remap_ipv4(unsigned long ipv4);

} // namespace ServerOverride
