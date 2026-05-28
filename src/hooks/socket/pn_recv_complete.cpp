#include "target_hooks.h"

#include "game/net/pn_fast_socket.hpp"

extern "C" void __declspec(naked) hook_pn_recv_complete() {
  __asm {
    jmp PNFastSocket::recv_complete
  }
}

HookStub g_target_pn_recv_complete = {pn::rva::kRecvComplete,
                                      (uint32_t)(uintptr_t)hook_pn_recv_complete,
                                      "hook_pn_recv_complete",
                                      {0},
                                      false,
                                      0};
