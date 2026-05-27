#include "game/net/pn_upnp.hpp"

#include "console.h"
#include "game/server_override.hpp"

using OrigFn = void(__thiscall *)(void *self);

static OrigFn g_orig_upnp_add_port_mapping =
    reinterpret_cast<OrigFn>(0xD6E187);

void PNUpnpClient::AddPortMapping() {
  if (ServerOverride::override_active()) {
    logf("PN connect: skip UPnP (offline server override)");
    return;
  }
  g_orig_upnp_add_port_mapping(this);
}
