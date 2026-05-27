#include "console.h"
#include "game/server_override.hpp"
#include "target_hooks.h"

#include "WinSock2.h"

void __cdecl handle_w_connect_3(DWORD *this_p, int namelen) {
  auto *name = reinterpret_cast<sockaddr *>(this_p + 1);
  if (namelen >= static_cast<int>(sizeof(sockaddr_in)) &&
      name->sa_family == AF_INET) {
    auto *in = reinterpret_cast<sockaddr_in *>(name);
    const u_short port = ntohs(in->sin_port);
    in_addr was = in->sin_addr;
    if (ServerOverride::remap_sockaddr_in(in) &&
        port == ServerOverride::kGameLegPort)
      logf("w_connect_3:27380 %s -> %s", inet_ntoa(was), inet_ntoa(in->sin_addr));
  }
}

extern "C" void __declspec(naked) hook_w_connect_3() {
  __asm {
    pushad // esp += 0x20
    pushfd // esp += 0x04

            // Push arguments in reverse order (__cdecl convention)
    push 0x10 // namelen
    push ecx // this
    
    call handle_w_connect_3

    add esp, 16

    popfd
    popad

        // Execute original instructions that were overwritten
    push 0x10
    lea eax, [ecx + 4]

    // Jump back to original code after our patch
    jmp [g_target_w_connect_3.return_addr]
  }
}

HookStub g_target_w_connect_3 = {
    0x014F6DC0,
    (uint32_t)(uintptr_t)hook_w_connect_3,
    "hook_w_connect_3",
    {0},
    false,
    0x014F6DC5,
};