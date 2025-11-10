#pragma once
#include <cstdint>
#include <windows.h>

struct HookStub {
  uint32_t addr;              // Virtual Address in target EXE
  uint32_t hook_function;     // RVA to our hook function in this DLL
  const char *name;           // Human-readable name for debugging
  uint8_t original_bytes[12]; // Store original instructions
  bool is_hooked;             // Track hook status
  uint32_t return_addr;       // addr to return after hook
};

class HookManager {
public:
  static bool initialize();
  static bool make_hook(HookStub &stub);
  static bool restore_hook(HookStub &stub);

private:
  static bool write_memory(void *address, const void *data, size_t size);
  static bool make_memory_writable(void *address, size_t size);
};