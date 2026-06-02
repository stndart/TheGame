#include "ProudNet/UpnpClient.hpp"

#include "game/server_override.hpp"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

void Proud::UpnpClient::AddPortMapping() {
  if (ServerOverride::override_active()) {
    if (!thegame::cfg.no_proud_logs)
      thegame::logpnf(0, "skip UPnP (offline server override)");
    return;
  }
  // sub_D6E180 is void; online paths unwind without a success flag in eax.
}

void Proud::UpnpClient::DeletePortMapping() {
  if (ServerOverride::override_active())
    return;
  // sub_D6E5B0 is void; same no-op contract as AddPortMapping.
}
