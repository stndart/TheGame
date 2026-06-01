#pragma once

#include <cstdint>
#include <string>

struct sockaddr_in;

namespace ServerOverride {

// Hostnames from encrypted options.dat
constexpr const char *kEntryHost = "137.184.201.52";

// ProudNet legs (ENTRY / GAME / PROBE) - remap these ports in offline mode.
constexpr uint16_t kEntryPort = 7000;
constexpr uint16_t kGameLegPort = 27380;
constexpr uint16_t kProbePort = 20009;

bool override_active();

// Patch hostname before socket construction
std::string remap_host(const char *host);

// Patch sockaddr_in in place; returns true when address changed.
bool remap_sockaddr_in(sockaddr_in *in);

} // namespace ServerOverride
