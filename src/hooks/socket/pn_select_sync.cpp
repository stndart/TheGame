#include "target_hooks.h"

#include "game/net/socket_trace.hpp"
#include "game/server_override.hpp"

// sub_D55300(__thiscall): ecx=fd_set*, [esp+4]=optval (CFastSocket ctx), [esp+8]=a3.
// Patched prologue: push ecx/esi/edi; mov edi,[esp+10h]; mov esi,ecx @ +7.

extern "C" void __cdecl pn_select_sync_optval(void *optval) {
  if (!ServerOverride::override_active() || !optval)
    return;
  SocketTrace::sync_fast_socket_handle(optval);
}

extern "C" void __declspec(naked) hook_pn_select() {
  __asm {
    push ecx
    push esi
    push edi
    mov eax, [esp + 10h]
    push eax
    call pn_select_sync_optval
    add esp, 4
    mov edi, [esp + 10h]
    mov ecx, [esp + 8]
    jmp dword ptr [g_target_pn_select.return_addr]
  }
}

HookStub g_target_pn_select = {0xD55300,
                               (uint32_t)(uintptr_t)hook_pn_select,
                               "hook_pn_select",
                               {0},
                               false,
                               0xD55307};
