#include "target_hooks.h"

#include <windows.h>

#include "ProudNet/Layout.hpp"
#include "thegame/log.hpp"

using thegame::logf;

namespace {

uintptr_t game_va(const uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

constexpr std::uint32_t kNetClientFactoryResume = 0xD0C0A7;

} // namespace

using NetClientCtorFn = void *(__thiscall *)(void *self);

extern "C" void log_pn_net_client_factory() {
  logf("CNetClient factory tid=%lu",
       static_cast<unsigned long>(GetCurrentThreadId()));
}

extern "C" void *__fastcall hook_pn_net_client_ctor_impl(void *self) {
  HookManager::restore_hook(g_target_pn_net_client_ctor);
  const auto orig = reinterpret_cast<NetClientCtorFn>(
      game_va(static_cast<uint32_t>(Proud::Rva::kNetClientCtor)));
  void *const ret = orig(self);
  HookManager::make_hook(g_target_pn_net_client_ctor);
  logf("CNetClient ctor tid=%lu this=%p ret=%p",
       static_cast<unsigned long>(GetCurrentThreadId()), self, ret);
  return ret;
}

// sub_D0C0A0 - CNetClient factory (__cdecl); SEH prologue, resume @ 0xD0C0A7.
extern "C" void __declspec(naked) hook_pn_net_client_factory() {
  __asm {
    pushad
    pushfd
    call log_pn_net_client_factory
    popfd
    popad

    push 0FFFFFFFFh
    push 1508A6Bh
    jmp [g_target_pn_net_client_factory.return_addr]
  }
}

// sub_D0A340 - CNetClient ctor (__thiscall); restore_hook for SEH entry.
extern "C" void __declspec(naked) hook_pn_net_client_ctor() {
  __asm {
    jmp hook_pn_net_client_ctor_impl
  }
}

HookStub g_target_pn_net_client_factory = {
    static_cast<uint32_t>(Proud::Rva::kNetClientFactory),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_net_client_factory)),
    "hook_pn_net_client_factory",
    {0},
    false,
    kNetClientFactoryResume,
};

HookStub g_target_pn_net_client_ctor = {
    static_cast<uint32_t>(Proud::Rva::kNetClientCtor),
    static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook_pn_net_client_ctor)),
    "hook_pn_net_client_ctor",
    {0},
    false,
    static_cast<uint32_t>(Proud::Rva::kNetClientCtor) + 5,
};
