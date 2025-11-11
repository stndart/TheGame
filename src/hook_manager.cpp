#include "hook_manager.h"
#include <minwindef.h>

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
  BYTE *target_address = reinterpret_cast<BYTE *>(stub.addr);

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