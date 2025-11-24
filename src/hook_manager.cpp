#include "hook_manager.h"
#include <cstdint>
#include <libloaderapi.h>
#include <minwindef.h>

#include <console.h>

bool HookManager::write_memory(void *address, const void *data, size_t size) {
  return WriteProcessMemory(GetCurrentProcess(), address,
                            const_cast<void *>(data), size, nullptr);
}

bool HookManager::make_memory_writable(void *address, size_t size) {
  DWORD old_protect;
  return VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect);
}

bool HookManager::make_hook(HookStub &stub) {
  if (stub.is_hooked)
    return true;

  // Get base address of main executable
  HMODULE main_module = GetModuleHandle(nullptr);
  if (!main_module)
    return false;

  // Calculate target address in main EXE
  BYTE *target_address = reinterpret_cast<BYTE *>(
      stub.addr + (int32_t)main_module - (int32_t)0x400000);

  // Save original bytes
  memcpy(stub.original_bytes, target_address, sizeof(stub.original_bytes));

  // Make memory writable
  if (!make_memory_writable(target_address, 12)) {
    return false;
  }

  // Calculate relative jump to our hook function
  BYTE *hook_address = reinterpret_cast<BYTE *>(stub.hook_function);
  int32_t relative_offset =
      static_cast<int32_t>(hook_address - (target_address + 5));

  // Create jump instruction: E9 [relative_offset]
  BYTE jump_patch[5];
  jump_patch[0] = 0xE9; // JMP instruction
  *reinterpret_cast<int32_t *>(&jump_patch[1]) = relative_offset;

  // Apply the patch
  if (!write_memory(target_address, jump_patch, sizeof(jump_patch))) {
    return false;
  }

  stub.is_hooked = true;
  HookManager::hooks.push_back(&stub);
  return true;
}

bool HookManager::restore_hook(HookStub &stub) {
  if (!stub.is_hooked)
    return true;

  HMODULE main_module = GetModuleHandle(nullptr);
  BYTE *target_address = reinterpret_cast<BYTE *>(stub.addr);

  if (!make_memory_writable(target_address, sizeof(stub.original_bytes))) {
    return false;
  }

  if (!write_memory(target_address, stub.original_bytes,
                    sizeof(stub.original_bytes))) {
    return false;
  }

  stub.is_hooked = false;
  return true;
}

bool HookManager::make_syshook(SysHookStub &stub, int32_t iat_addr) {
  return make_syshook(stub, reinterpret_cast<void *>(iat_addr));
}

bool HookManager::make_syshook(SysHookStub &stub, void *iat_addr) {
  HMODULE main_module = GetModuleHandle(nullptr);
  if (!main_module)
    return false;

  stub.patched_addr =
      reinterpret_cast<BYTE *>(reinterpret_cast<uintptr_t>(iat_addr) +
                               (int32_t)main_module - (int32_t)0x400000);
  HMODULE module = GetModuleHandle(stub.modname);
  if (!module) {
    logf("Failed to get module %s", stub.modname);
    return false;
  }

  stub.sym_addr =
      reinterpret_cast<BYTE *>(GetProcAddress(module, stub.symname));

  // Make memory writable
  if (!make_memory_writable(stub.patched_addr, 4)) {
    return false;
  }

  DWORD iat_patch = reinterpret_cast<DWORD>(stub.handler);
  DWORD old_iat;

  memcpy(&old_iat, stub.patched_addr, sizeof(old_iat));
  if (old_iat != (DWORD)stub.sym_addr)
    logf("old iat of %s is %p, while resolved is %p", stub.symname, old_iat,
         stub.sym_addr);

  if (!write_memory(stub.patched_addr, &iat_patch, sizeof(iat_patch))) {
    return false;
  }

  logf("Hooked system call to %s_%s through IAT at %p", stub.modname,
       stub.symname, stub.patched_addr);

  stub.is_hooked = true;
  HookManager::syshooks.push_back(&stub);
  return true;
}

bool HookManager::restore_syshook(SysHookStub &stub) {
  if (!stub.is_hooked)
    return true;

  // Make memory writable
  if (!make_memory_writable(stub.patched_addr, 4)) {
    return false;
  }

  if (!write_memory(stub.patched_addr, &stub.sym_addr, sizeof(stub.sym_addr))) {
    return false;
  }

  stub.is_hooked = false;
  return true;
}

bool HookManager::restore_all_hooks() {
  bool status = true;
  for (auto hook : HookManager::hooks) {
    if (hook) {
      status = status && HookManager::restore_hook(*hook);
    }
  }
  return status;
}

bool HookManager::initialize() { return true; }

std::vector<HookStub *> HookManager::hooks;
std::vector<SysHookStub *> HookManager::syshooks;