#include <windows.h>

#include "console.h"
#include "crt/memory.h"

CRT::CRTStubs CRT::stub;

void init_msvcr90(CRT::CRTStubs &stub) {
  HMODULE crt_mod = GetModuleHandle("msvcr90.dll");
  if (!crt_mod) {
    logf("Failed to get msvcr90");
    return;
  }

  stub.recalloc = reinterpret_cast<CRT::CRTStubs::recalloc_t>(
      GetProcAddress(crt_mod, "_recalloc"));
  if (!stub.recalloc) {
    logf("Failed to get recalloc");
  } else {
    logf("recalloc 0x%p", stub.recalloc);
  }

  stub.calloc = reinterpret_cast<CRT::CRTStubs::calloc_t>(
      GetProcAddress(crt_mod, "calloc"));
  if (!stub.calloc) {
    logf("Failed to get calloc");
  } else {
    logf("calloc 0x%p", stub.calloc);
  }

  stub.free =
      reinterpret_cast<CRT::CRTStubs::free_t>(GetProcAddress(crt_mod, "free"));
  if (!stub.free) {
    logf("Failed to get free");
  } else {
    logf("free 0x%p", stub.free);
  }
}

void init_kernel32(CRT::CRTStubs &stub) {
  HMODULE k32_mod = GetModuleHandle("kernel32.dll");
  if (!k32_mod) {
    logf("Failed to get kernel32");
    return;
  }

  stub.get_proc_heap = reinterpret_cast<CRT::CRTStubs::get_proc_heap_t>(
      GetProcAddress(k32_mod, "GetProcessHeap"));
  if (!stub.get_proc_heap) {
    logf("Failed to get get_proc_heap");
  } else {
    logf("get_proc_heap 0x%p", stub.get_proc_heap);
  }

  stub.heap_free = reinterpret_cast<CRT::CRTStubs::heap_free_t>(
      GetProcAddress(k32_mod, "HeapFree"));
  if (!stub.heap_free) {
    logf("Failed to get heap_free");
  } else {
    logf("heap_free 0x%p", stub.heap_free);
  }
}

void CRT::init_CRT() {
  logf("Legacy CRT init...");

  init_msvcr90(stub);
  init_kernel32(stub);
}

void *__cdecl CRT::recalloc(void *ptr, size_t _Count, size_t _Size) {
  return stub.recalloc(ptr, _Count, _Size);
}

void *__cdecl CRT::calloc(size_t _Count, size_t _Size) {
  return stub.calloc(_Count, _Size);
}

void __cdecl CRT::free(void *_Block) { return stub.free(_Block); }

HANDLE __stdcall CRT::get_proc_heap() { return stub.get_proc_heap(); }

bool __stdcall CRT::heap_free(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
  return stub.heap_free(hHeap, dwFlags, lpMem);
}
