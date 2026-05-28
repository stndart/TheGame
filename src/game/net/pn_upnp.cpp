#include "game/net/pn_upnp.hpp"

#include "console.h"
#include "game/server_override.hpp"

void PNUpnpClient::AddPortMapping() {
  if (ServerOverride::override_active()) {
    logf("PN connect: skip UPnP (offline server override)");
    return;
  }
  // sub_D6E180 is void; online paths unwind without a success flag in eax.
}

void PNUpnpClient::DeletePortMapping() {
  if (ServerOverride::override_active())
    return;
  // sub_D6E5B0 is void; same no-op contract as AddPortMapping.
}
