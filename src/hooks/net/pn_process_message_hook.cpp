#include "target_hooks.h"

#include "ProudNet/Layout.hpp"
#include "ProudNet/ProcessProudNetLayer.hpp"
#include "console.h" // IWYU pragma: keep

// sub_D653B0: char __thiscall(worker, wstr_body, received_msg)
// SEH prologue (7 bytes): 6A FF | 68 11 2E 51 01 | then @ 0xD653B7: mov eax,
// fs:[0] 5-byte JMP steals 6A FF 68 11 2E - resume at 0xD653B7 (not RVA+5).

namespace {

constexpr std::uint32_t kProcessProudNetLayerResume = 0xD653B7;

} // namespace

extern "C" char __cdecl
pn_process_message_reimpl_c(void *worker, void *wstr_body, void *received_msg) {
  return Proud::ProcessMessageProudNetLayer(worker, wstr_body, received_msg)
             ? 1
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

HookStub g_target_pn_process_proudnet_layer = {
    static_cast<uint32_t>(Proud::Rva::kProcessProudNetLayer),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_process_proudnet_layer)),
    "hook_pn_process_proudnet_layer",
    {0},
    false,
    kProcessProudNetLayerResume,
};
