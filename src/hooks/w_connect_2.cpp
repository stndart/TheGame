#include "console.h"
#include "game/server_override.hpp"
#include "system_hooks.h"
#include "target_hooks.h"

#include "WinSock2.h"

// w_connect_2 @ 0x14314E0 — ProudNet alternate connect path (__fastcall, ~591 B).
// Patches the connect preamble at 0x14316A2 (5 B); IAT call at +0xB is 6 B (not patchable).

void __cdecl handle_w_connect_2_connect(SOCKET s, sockaddr *name, int namelen) {
  (void)s;
  if (namelen >= static_cast<int>(sizeof(sockaddr_in)) &&
      name->sa_family == AF_INET) {
    auto *in = reinterpret_cast<sockaddr_in *>(name);
    const u_short port = ntohs(in->sin_port);
    in_addr was = in->sin_addr;
    if (ServerOverride::remap_sockaddr_in(in) &&
        port == ServerOverride::kGameLegPort)
      logf("w_connect_2:27380 %s -> %s", inet_ntoa(was), inet_ntoa(in->sin_addr));
  }
}

extern "C" void __declspec(naked) hook_w_connect_2_connect() {
  __asm {
    mov edx, [esp + 0x2C]
    push edx
    lea eax, [esp + 0x34]
    push esi
    push eax
    push edx
    call handle_w_connect_2_connect
    add esp, 12

    push dword ptr [esp + 8]
    push dword ptr [esp + 4]
    push dword ptr [esp]
    call connect_syshandle

    jmp dword ptr [g_target_w_connect_2.return_addr]
  }
}

HookStub g_target_w_connect_2 = {
    0x014316A2,
    (uint32_t)(uintptr_t)hook_w_connect_2_connect,
    "hook_w_connect_2",
    {0},
    false,
    0x014316B3,
};
