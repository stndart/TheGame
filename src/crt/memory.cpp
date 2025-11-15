#include <windows.h>

#include "console.h"
#include "crt/memory.h"

CRT::CRTStubs CRT::stub;

void init_msvcr90(CRT::CRTStubs &stub) {
  HMODULE crt_mod = GetModuleHandle("msvcr90.dll");
  if (!crt_mod) {
    OutputDebugString("Failed to get msvcr90");
    return;
  }

  stub.recalloc = reinterpret_cast<CRT::CRTStubs::recalloc_t>(
      GetProcAddress(crt_mod, "_recalloc"));
  if (!stub.recalloc) {
    OutputDebugString("Failed to get recalloc");
  } else {
    OutputDebugString("recalloc found"); // stub.recalloc);
  }

  stub.calloc = reinterpret_cast<CRT::CRTStubs::calloc_t>(
      GetProcAddress(crt_mod, "calloc"));
  if (!stub.calloc) {
    OutputDebugString("Failed to get calloc");
  } else {
    OutputDebugString("calloc found"); // stub.calloc);
  }

  stub.free =
      reinterpret_cast<CRT::CRTStubs::free_t>(GetProcAddress(crt_mod, "free"));
  if (!stub.free) {
    OutputDebugString("Failed to get free");
  } else {
    OutputDebugString("free found"); // stub.free);
  }
}

void init_kernel32(CRT::CRTStubs &stub) {
  HMODULE k32_mod = GetModuleHandle("kernel32.dll");
  if (!k32_mod) {
    OutputDebugString("Failed to get kernel32");
    return;
  }

  stub.get_proc_heap = reinterpret_cast<CRT::CRTStubs::get_proc_heap_t>(
      GetProcAddress(k32_mod, "GetProcessHeap"));
  if (!stub.get_proc_heap) {
    OutputDebugString("Failed to get get_proc_heap");
  } else {
    OutputDebugString("get_proc_heap found"); // stub.get_proc_heap);
  }

  stub.heap_free = reinterpret_cast<CRT::CRTStubs::heap_free_t>(
      GetProcAddress(k32_mod, "HeapFree"));
  if (!stub.heap_free) {
    OutputDebugString("Failed to get heap_free");
  } else {
    OutputDebugString("heap_free found"); // stub.heap_free);
  }

  stub.heap_alloc = reinterpret_cast<CRT::CRTStubs::heap_alloc_t>(
      GetProcAddress(k32_mod, "HeapAlloc"));
  if (!stub.heap_alloc) {
    OutputDebugString("Failed to get heap_alloc");
  } else {
    OutputDebugString("heap_alloc found"); // stub.heap_alloc);
  }
}

void CRT::init_CRT() {
  OutputDebugString("Legacy CRT init...");

  init_msvcr90(stub);
  init_kernel32(stub);

  stub.initialized = true;
}

void *__cdecl CRT::recalloc(void *ptr, size_t _Count, size_t _Size) {
  if (!stub.initialized)
    init_CRT();
  return stub.recalloc(ptr, _Count, _Size);
}

void *__cdecl CRT::calloc(size_t _Count, size_t _Size) {
  if (!stub.initialized)
    init_CRT();
  return stub.calloc(_Count, _Size);
}

void __cdecl CRT::free(void *_Block) {
  if (!stub.initialized)
    init_CRT();
  return stub.free(_Block);
}

HANDLE __stdcall CRT::get_proc_heap() {
  if (!stub.initialized)
    init_CRT();
  return stub.get_proc_heap();
}

bool __stdcall CRT::heap_free(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
  if (!stub.initialized)
    init_CRT();
  return stub.heap_free(hHeap, dwFlags, lpMem);
}
void *__stdcall CRT::heap_alloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
  if (!stub.initialized)
    init_CRT();
  return stub.heap_alloc(hHeap, dwFlags, dwBytes);
}

// Override global new/delete operators to use your custom allocators
void *operator new(size_t size) {
  if (!CRT::stub.initialized)
    CRT::init_CRT();
  return CRT::stub.recalloc(nullptr, size, 1);
}

void *operator new[](size_t size) {
  if (!CRT::stub.initialized)
    CRT::init_CRT();
  return CRT::stub.recalloc(nullptr, size, 1);
}

void operator delete(void *ptr) noexcept {
  if (!CRT::stub.initialized)
    CRT::init_CRT();
  if (ptr)
    CRT::stub.recalloc(ptr, 0, 0); // Using recalloc with 0 size as free
}

void operator delete[](void *ptr) noexcept {
  if (!CRT::stub.initialized)
    CRT::init_CRT();
  if (ptr)
    CRT::stub.recalloc(ptr, 0, 0);
}

void operator delete(void *ptr, size_t) noexcept {
  if (!CRT::stub.initialized)
    CRT::init_CRT();
  if (ptr)
    CRT::stub.recalloc(ptr, 0, 0);
}