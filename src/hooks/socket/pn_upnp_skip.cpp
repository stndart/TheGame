#include "target_hooks.h"

#include "game/net/pn_upnp.hpp"

// sub_D6E180 - UPnP AddPortMapping SOAP before first PN send (state 1).

extern "C" void __declspec(naked) hook_pn_upnp() {
  __asm {
    jmp PNUpnpClient::AddPortMapping
  }
}

HookStub g_target_pn_upnp = {0xD6E180,       (uint32_t)(uintptr_t)hook_pn_upnp,
                             "hook_pn_upnp", {0},
                             false,          0xD6E187};
