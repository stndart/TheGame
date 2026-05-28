#include "target_hooks.h"

#include "game/net/pn_recv_append.hpp"

// sub_D71CE0 - __thiscall append of completed WSARecv bytes (PN FSM state 3).

extern "C" void __declspec(naked) hook_pn_recv_append() {
  __asm {
    jmp PNRecvBuffer::append
  }
}

HookStub g_target_pn_recv_append = {0xD71CE0,
                                    (uint32_t)(uintptr_t)hook_pn_recv_append,
                                    "hook_pn_recv_append",
                                    {0},
                                    false,
                                    0};
