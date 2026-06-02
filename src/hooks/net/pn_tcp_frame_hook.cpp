#include "target_hooks.h"

#include <windows.h>

#include "ProudNet/Layout.hpp"
#include "ProudNet/TcpLayerMessageExtractor.hpp"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

using thegame::logpnf;

namespace {

constexpr int kLogLimit = 8;

// IDA @ 0xD84BB0: 6A FF 68 0F 6B 51 01 | 64 A1 00 00 00 00 ...
// 5-byte hook patch ends inside the push imm32; resume at 0xD84BB7.
constexpr std::uint32_t kTcpFrameRecvResume = 0xD84BB7u;

uintptr_t game_va(const std::uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

void *g_tcp_frame_send_body = nullptr;

void ensure_tcp_frame_send_body() {
  if (!g_tcp_frame_send_body) {
    // Send entry uses the same SEH prologue; +5 is unsafe (resume @ 0xD84977).
    g_tcp_frame_send_body = reinterpret_cast<void *>(game_va(0xD84977u));
  }
}

} // namespace

extern "C" char tcp_frame_extract_c(void *self, void *stream_ctx) {
  return reinterpret_cast<Proud::CTcpLayerMessageExtractor *>(self)->extract(
      stream_ctx);
}

// Full replacement: __thiscall (ecx=this, [esp+4]=stream_ctx, ret 4).
// Trampoline alternative after hook: push 0FFFFFFFFh; push 01516B0Fh; jmp
// return_addr.
extern "C" void __declspec(naked) hook_pn_tcp_frame_recv() {
  __asm {
    push dword ptr [esp + 4]
    push ecx
    call tcp_frame_extract_c
    add esp, 8
    ret 4
  }
}

extern "C" void log_tcp_frame_send_once(void *framer) {
  ensure_tcp_frame_send_body();
  static int logs = 0;
  if (logs < kLogLimit && !thegame::cfg.no_proud_logs) {
    logpnf(0, "tcp_frame: send self=%p", framer);
    ++logs;
  }
}

// sub_D84970 - tail only after SEH push pair (not RVA+5).
extern "C" void __declspec(naked) hook_pn_tcp_frame_send() {
  __asm {
    push ecx
    call log_tcp_frame_send_once
    add esp, 4
    pop ecx
    jmp dword ptr [g_tcp_frame_send_body]
  }
}

HookStub g_target_pn_tcp_frame_recv = {
    static_cast<uint32_t>(Proud::Rva::kTcpFrameRecvHook),
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook_pn_tcp_frame_recv)),
    "hook_pn_tcp_frame_recv",
    {0},
    false,
    kTcpFrameRecvResume,
};

HookStub g_target_pn_tcp_frame_send = {
    static_cast<uint32_t>(Proud::Rva::kTcpFrameSendHook),
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook_pn_tcp_frame_send)),
    "hook_pn_tcp_frame_send",
    {0},
    false,
    0xD84977u,
};
