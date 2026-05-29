#include "target_hooks.h"

#include "console.h"
#include "game/net/pn_layout.hpp"

#ifndef THEGAME_PN_PROCESS_FULL_REIMPL
#define THEGAME_PN_PROCESS_FULL_REIMPL 1
#endif

#if THEGAME_PN_PROCESS_FULL_REIMPL
#include "game/net/pn_process_message.hpp"
#endif

// sub_D653B0: char __thiscall(worker, wstr_body, received_msg)
// SEH prologue (7 bytes): 6A FF | 68 11 2E 51 01 | then @ 0xD653B7: mov eax,
// fs:[0] 5-byte JMP steals 6A FF 68 11 2E - resume at 0xD653B7 (not RVA+5).

namespace {

constexpr std::uint32_t kProcessProudNetLayerResume = 0xD653B7;

} // namespace

extern "C" void __cdecl pn_process_message_trace_log(void *worker,
                                                     void *wstr_body,
                                                     void *received_msg) {
  static unsigned int trace_count = 0;
  const unsigned int n = ++trace_count;
  if (n <= 32u || (n % 256u) == 0u) {
    logf("ProcessMessage_ProudNetLayer trace #%u worker=%p wstr=%p msg=%p", n,
         worker, wstr_body, received_msg);
  }
}

#if THEGAME_PN_PROCESS_FULL_REIMPL

extern "C" char __cdecl
pn_process_message_reimpl_c(void *worker, void *wstr_body, void *received_msg) {
  return process_message_proudnet_layer(worker, wstr_body, received_msg) ? 1
                                                                         : 0;
}

extern "C" void __declspec(naked) hook_pn_process_proudnet_layer() {
  __asm {
    push dword ptr [esp + 8]
    push dword ptr [esp + 8]
    push ecx
    call pn_process_message_reimpl_c
    add esp, 12
    ret 8
  }
}

#else

extern "C" void __declspec(naked) hook_pn_process_proudnet_layer() {
  __asm {
    mov eax, ecx
    mov ebx, dword ptr [esp + 4]
    mov edx, dword ptr [esp + 8]
    pushad
    pushfd
    push edx
    push ebx
    push eax
    call pn_process_message_trace_log
    add esp, 12
    popfd
    popad

    push 0FFFFFFFFh
    push 1512E11h
    jmp g_target_pn_process_proudnet_layer.return_addr
  }
}

#endif

HookStub g_target_pn_process_proudnet_layer = {
    static_cast<uint32_t>(pn::rva::kProcessProudNetLayer),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_process_proudnet_layer)),
    "hook_pn_process_proudnet_layer",
    {0},
    false,
    kProcessProudNetLayerResume,
};
