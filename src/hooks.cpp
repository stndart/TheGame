#include "hook_manager.h"

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
  uint8_t *target_address = reinterpret_cast<uint8_t *>(
      reinterpret_cast<uintptr_t>(main_module) + stub.rva);

  // Calculate our hook function address in this DLL
  HMODULE this_dll = GetModuleHandle("TheGame.dll"); // Match your DLL name
  if (!this_dll)
    return false;

  uint8_t *hook_address = reinterpret_cast<uint8_t *>(
      reinterpret_cast<uintptr_t>(this_dll) + stub.hook_function);

  // Calculate relative jump offset
  int32_t relative_offset =
      static_cast<int32_t>(hook_address - (target_address + 5));

  // Save original bytes
  memcpy(stub.original_bytes, target_address, sizeof(stub.original_bytes));

  // Make memory writable
  if (!make_memory_writable(target_address, 12)) {
    return false;
  }

  // Create jump instruction: E9 [relative_offset]
  uint8_t jump_patch[5];
  jump_patch[0] = 0xE9; // JMP instruction
  *reinterpret_cast<int32_t *>(&jump_patch[1]) = relative_offset;

  // Apply the patch
  if (!write_memory(target_address, jump_patch, sizeof(jump_patch))) {
    return false;
  }

  stub.is_hooked = true;
  return true;
}

bool HookManager::restore_hook(HookStub &stub) {
  if (!stub.is_hooked)
    return true;

  HMODULE main_module = GetModuleHandle(nullptr);
  uint8_t *target_address = reinterpret_cast<uint8_t *>(
      reinterpret_cast<uintptr_t>(main_module) + stub.rva);

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

bool HookManager::initialize() {
  // Initialize any required state
  return true;
}