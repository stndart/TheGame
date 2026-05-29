#include "target_hooks.h"

#include "game/net/pn_drain_recv.hpp"
#include "game/net/pn_layout.hpp"

#include <cstring>
#include <windows.h>

namespace {

uintptr_t game_va(const std::uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

void *g_drain_receive_queue_resume = nullptr;
std::uint32_t g_drain_seh_handler = 0;

void ensure_drain_tail() {
  if (!g_drain_receive_queue_resume) {
    g_drain_receive_queue_resume = reinterpret_cast<void *>(
        game_va(static_cast<std::uint32_t>(pn::rva::kDrainReceiveQueueResume)));
    std::memcpy(&g_drain_seh_handler,
                &g_target_pn_drain_receive_queue.original_bytes[3],
                sizeof(std::uint32_t));
  }
}

} // namespace

extern "C" void drain_receive_queue_log(void *net_client) {
  ensure_drain_tail();
  drain_receive_queue(net_client);
}

// sub_D65940 — trace + SEH tail @ 0xD65947 (original body continues; ECX preserved).
extern "C" void __declspec(naked) hook_pn_drain_receive_queue() {
  __asm {
    push ecx
    push ecx
    call drain_receive_queue_log
    add esp, 4
    pop ecx
    push 0FFFFFFFFh
    push g_drain_seh_handler
    jmp dword ptr [g_drain_receive_queue_resume]
  }
}

HookStub g_target_pn_drain_receive_queue = {
    static_cast<uint32_t>(pn::rva::kDrainReceiveQueue),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_drain_receive_queue)),
    "hook_pn_drain_receive_queue",
    {0},
    false,
    static_cast<uint32_t>(pn::rva::kDrainReceiveQueueResume),
};
