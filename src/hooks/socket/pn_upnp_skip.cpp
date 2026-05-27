#include "target_hooks.h"

#include "console.h"
#include "game/server_override.hpp"

// sub_D6E180 - UPnP AddPortMapping SOAP before first PN send (state 1). Blocks
// offline.

extern "C" int __cdecl pn_should_skip_upnp() {
  return ServerOverride::override_active() ? 1 : 0;
}

extern "C" void __cdecl pn_log_skip_upnp() {
  logf("PN connect: skip UPnP (offline server override)");
}

extern "C" void __declspec(naked) hook_pn_upnp() {
  __asm {
    pushad
    pushfd
    call pn_should_skip_upnp
    test eax, eax
    jnz skip_upnp
    popfd
    popad
    jmp dword ptr [g_target_pn_upnp.return_addr]
  skip_upnp:
    call pn_log_skip_upnp
    popfd
    popad
    ret
  }
}

HookStub g_target_pn_upnp = {0xD6E180,       (uint32_t)(uintptr_t)hook_pn_upnp,
                             "hook_pn_upnp", {0},
                             false,          0xD6E187};
