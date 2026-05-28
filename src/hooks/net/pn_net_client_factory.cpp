#include "hook_manager.h"
#include "target_hooks.h"

#include "console.h"
#include "game/net/pn_layout.hpp"

#include <windows.h>

namespace {

uintptr_t game_va(const uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

} // namespace

using NetClientFactoryFn = void *(__cdecl *)();
using NetClientCtorFn = void *(__thiscall *)(void *self);

extern "C" void *__cdecl hook_pn_net_client_factory() {
  HookManager::restore_hook(g_target_pn_net_client_factory);
  const auto orig = reinterpret_cast<NetClientFactoryFn>(
      game_va(static_cast<uint32_t>(pn::rva::kNetClientFactory)));
  void *const ret = orig();
  HookManager::make_hook(g_target_pn_net_client_factory);
  logf("CNetClient factory tid=%lu ret=%p",
       static_cast<unsigned long>(GetCurrentThreadId()), ret);
  return ret;
}

extern "C" void *__fastcall hook_pn_net_client_ctor_impl(void *self) {
  HookManager::restore_hook(g_target_pn_net_client_ctor);
  const auto orig = reinterpret_cast<NetClientCtorFn>(
      game_va(static_cast<uint32_t>(pn::rva::kNetClientCtor)));
  void *const ret = orig(self);
  HookManager::make_hook(g_target_pn_net_client_ctor);
  logf("CNetClient ctor tid=%lu this=%p ret=%p",
       static_cast<unsigned long>(GetCurrentThreadId()), self, ret);
  return ret;
}

extern "C" void __declspec(naked) hook_pn_net_client_ctor() {
  __asm {
    jmp hook_pn_net_client_ctor_impl
  }
}

HookStub g_target_pn_net_client_factory = {
    static_cast<uint32_t>(pn::rva::kNetClientFactory),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_net_client_factory)),
    "hook_pn_net_client_factory",
    {0},
    false,
    static_cast<uint32_t>(pn::rva::kNetClientFactory) + 5,
};

HookStub g_target_pn_net_client_ctor = {
    static_cast<uint32_t>(pn::rva::kNetClientCtor),
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook_pn_net_client_ctor)),
    "hook_pn_net_client_ctor",
    {0},
    false,
    static_cast<uint32_t>(pn::rva::kNetClientCtor) + 5,
};
