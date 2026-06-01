#include "ProudNet/TcpLayerMessageExtractor.hpp"

#include <windows.h>

#include "ProudNet/Layout.hpp"
#include "hook_manager.h"
#include "target_hooks.h"
#include "thegame/log.hpp"

using thegame::logf;

namespace {

constexpr int kLogLimit = 8;

uintptr_t game_va(const std::uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

using TcpFrameExtractFn = char(__thiscall *)(void *extractor, void *stream_ctx);

} // namespace

char Proud::CTcpLayerMessageExtractor::extract(void *stream_ctx) {
  static int logs = 0;
  if (logs < kLogLimit) {
    logf("pn_tcp_frame: extract self=%p ctx=%p", this, stream_ctx);
    ++logs;
  }

  // sub_D84BB0 entry - do not tail-call RVA+5 (5-byte jmp splits SEH prologue).
  HookManager::restore_hook(g_target_pn_tcp_frame_recv);
  const auto orig = reinterpret_cast<TcpFrameExtractFn>(
      game_va(static_cast<std::uint32_t>(Proud::Rva::kTcpFrameRecv)));
  const char result = orig(this, stream_ctx);
  HookManager::make_hook(g_target_pn_tcp_frame_recv);
  return result;
}

void Proud::CTcpLayerMessageExtractor::frame_send(void *arg0, void *arg1) {
  (void)arg0;
  (void)arg1;
}
