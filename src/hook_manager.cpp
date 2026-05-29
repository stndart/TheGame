#include "hook_manager.h"
#include <libloaderapi.h>
#include <minwindef.h>
#include <winnt.h>

#include <console.h>

#include <cstring>

namespace {

CRITICAL_SECTION g_hook_patch_cs;
bool g_hook_patch_cs_ready = false;

void ensure_hook_patch_cs() {
  if (!g_hook_patch_cs_ready) {
    InitializeCriticalSection(&g_hook_patch_cs);
    g_hook_patch_cs_ready = true;
  }
}

struct HookPatchLock {
  HookPatchLock() {
    ensure_hook_patch_cs();
    EnterCriticalSection(&g_hook_patch_cs);
  }
  ~HookPatchLock() { LeaveCriticalSection(&g_hook_patch_cs); }
};

} // namespace

bool HookManager::write_memory(void *address, const void *data, size_t size) {
  return WriteProcessMemory(GetCurrentProcess(), address,
                            const_cast<void *>(data), size, nullptr);
}

bool HookManager::make_memory_writable(void *address, size_t size) {
  DWORD old_protect;
  return VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect);
}

bool HookManager::make_hook(HookStub &stub) {
  HookPatchLock lock;
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
  HookPatchLock lock;
  if (!stub.is_hooked)
    return true;

  HMODULE main_module = GetModuleHandle(nullptr);
  BYTE *target_address = reinterpret_cast<BYTE *>(
      stub.addr + (int32_t)main_module - (int32_t)0x400000);

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

void HookManager::invoke_hooked(HookStub &stub, void (*fn)(void *ctx),
                                void *ctx) {
  HookPatchLock lock;
  HMODULE main_module = GetModuleHandle(nullptr);
  if (!main_module || !fn)
    return;

  BYTE *target_address = reinterpret_cast<BYTE *>(
      stub.addr + (int32_t)main_module - (int32_t)0x400000);

  if (stub.is_hooked) {
    if (!make_memory_writable(target_address, sizeof(stub.original_bytes)))
      return;
    if (!write_memory(target_address, stub.original_bytes,
                      sizeof(stub.original_bytes))) {
      return;
    }
    stub.is_hooked = false;
  }

  fn(ctx);

  if (!stub.is_hooked) {
    if (!make_memory_writable(target_address, 12))
      return;

    BYTE *hook_address = reinterpret_cast<BYTE *>(stub.hook_function);
    const int32_t relative_offset =
        static_cast<int32_t>(hook_address - (target_address + 5));

    BYTE jump_patch[5];
    jump_patch[0] = 0xE9;
    *reinterpret_cast<int32_t *>(&jump_patch[1]) = relative_offset;

    if (!write_memory(target_address, jump_patch, sizeof(jump_patch)))
      return;

    stub.is_hooked = true;
  }
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
  for (auto syshook : HookManager::syshooks) {
    if (syshook) {
      status = status && HookManager::restore_syshook(*syshook);
    }
  }
  return status;
}

bool HookManager::hook_import(const HMODULE image, const char *import_dll,
                              const char *symbol, void *detour) {
  if (!image || !import_dll || !symbol || !detour)
    return false;

  const auto *dos = reinterpret_cast<PIMAGE_DOS_HEADER>(image);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  const auto *nt = reinterpret_cast<PIMAGE_NT_HEADERS>(
      reinterpret_cast<BYTE *>(image) + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE)
    return false;

  const auto &dir =
      nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!dir.VirtualAddress)
    return false;

  auto *imp = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
      reinterpret_cast<BYTE *>(image) + dir.VirtualAddress);

  bool patched = false;
  for (; imp->Name; ++imp) {
    const char *dll_name = reinterpret_cast<const char *>(
        reinterpret_cast<BYTE *>(image) + imp->Name);
    if (_stricmp(dll_name, import_dll) != 0)
      continue;

    auto *thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
        reinterpret_cast<BYTE *>(image) + imp->FirstThunk);
    auto *orig_thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(
        reinterpret_cast<BYTE *>(image) + imp->OriginalFirstThunk);
    if (!orig_thunk)
      orig_thunk = thunk;

    for (; orig_thunk->u1.AddressOfData; ++orig_thunk, ++thunk) {
      if (IMAGE_SNAP_BY_ORDINAL(orig_thunk->u1.Ordinal))
        continue;

      const auto *import_by_name = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
          reinterpret_cast<BYTE *>(image) + orig_thunk->u1.AddressOfData);
      if (strcmp(import_by_name->Name, symbol) != 0)
        continue;

      if (!make_memory_writable(&thunk->u1.Function, sizeof(thunk->u1.Function)))
        return false;

      thunk->u1.Function = reinterpret_cast<ULONG_PTR>(detour);
      patched = true;
      logf("hook_import: %s!%s at %p", dll_name, symbol, &thunk->u1.Function);
    }
  }
  return patched;
}

bool HookManager::initialize() { return true; }

std::vector<HookStub *> HookManager::hooks;
std::vector<SysHookStub *> HookManager::syshooks;