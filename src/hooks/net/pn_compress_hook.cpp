#include "hook_manager.h"
#include "target_hooks.h"

#include "ProudNet/Layout.hpp"
#include "console.h" // IWYU pragma: keep

#include <windows.h>

namespace {

constexpr int kLogLimit = 8;

uintptr_t game_va(const std::uint32_t va) {
  const uintptr_t delta =
      reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<uintptr_t>(va) + delta;
}

using ProcessCompressedFn = char(__thiscall *)(void *core, std::uint32_t type,
                                               void *received_msg,
                                               void *stack_mem);
using ProcessEncryptedFn = char(__thiscall *)(void *core, void *received_msg,
                                              void *stack_mem);

} // namespace

// sub_D5DC10 - __thiscall (ECX=core). SEH: 6A FF 68 … | resume @ 0xD5DC17.
// Passthrough: restore_hook then call full entry RVA (not RVA+5).
extern "C" char process_compressed_c(void *core, std::uint32_t type,
                                     void *received_msg, void *stack_mem) {
  static int logs = 0;
  if (logs < kLogLimit) {
    logf("pn_compress: core=%p type=%u msg=%p stack=%p", core, type,
         received_msg, stack_mem);
    ++logs;
  }

  HookManager::restore_hook(g_target_pn_process_compressed);
  const auto orig = reinterpret_cast<ProcessCompressedFn>(
      game_va(static_cast<std::uint32_t>(Proud::Rva::kProcessCompressed)));
  const char result = orig(core, type, received_msg, stack_mem);
  HookManager::make_hook(g_target_pn_process_compressed);
  return result;
}

extern "C" void __declspec(naked) hook_pn_process_compressed() {
  __asm {
    push dword ptr [esp + 0Ch]
    push dword ptr [esp + 0Ch]
    push dword ptr [esp + 8]
    push ecx
    call process_compressed_c
    add esp, 16
    ret 0Ch
  }
}

// sub_D5CA30 - __thiscall (ECX=core). SEH resume @ 0xD5CA37.
extern "C" char process_encrypted_c(void *core, void *received_msg,
                                    void *stack_mem) {
  static int logs = 0;
  if (logs < kLogLimit) {
    logf("pn_encrypt: core=%p msg=%p stack=%p", core, received_msg, stack_mem);
    ++logs;
  }

  HookManager::restore_hook(g_target_pn_process_encrypted);
  const auto orig = reinterpret_cast<ProcessEncryptedFn>(
      game_va(static_cast<std::uint32_t>(Proud::Rva::kProcessEncrypted)));
  const char result = orig(core, received_msg, stack_mem);
  HookManager::make_hook(g_target_pn_process_encrypted);
  return result;
}

extern "C" void __declspec(naked) hook_pn_process_encrypted() {
  __asm {
    push dword ptr [esp + 8]
    push dword ptr [esp + 8]
    push ecx
    call process_encrypted_c
    add esp, 12
    ret 8
  }
}

HookStub g_target_pn_process_compressed = {
    static_cast<uint32_t>(Proud::Rva::kProcessCompressed),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_process_compressed)),
    "hook_pn_process_compressed",
    {0},
    false,
    static_cast<uint32_t>(Proud::Rva::kProcessCompressedResume),
};

HookStub g_target_pn_process_encrypted = {
    static_cast<uint32_t>(Proud::Rva::kProcessEncrypted),
    static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(hook_pn_process_encrypted)),
    "hook_pn_process_encrypted",
    {0},
    false,
    static_cast<uint32_t>(Proud::Rva::kProcessEncryptedResume),
};
