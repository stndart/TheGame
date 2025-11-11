#define _X86_
#include <windows.h>

#include "console.h"
#include "crt/memory.h"

CRT::CRTStubs CRT::stub;

void CRT::init_CRT() {
  logf("Legacy CRT init...");

  HMODULE crt_mod = GetModuleHandle("msvcr90.dll");
  if (!crt_mod) {
    logf("Failed to get msvcrt90");
    return;
  }

  stub.recalloc = reinterpret_cast<CRTStubs::recalloc_t>(
      GetProcAddress(crt_mod, "_recalloc"));
  if (!stub.recalloc) {
    logf("Failed to get recalloc");
  } else {
    logf("recalloc 0x%p", stub.recalloc);
  }

  stub.calloc =
      reinterpret_cast<CRTStubs::calloc_t>(GetProcAddress(crt_mod, "calloc"));
  if (!stub.calloc) {
    logf("Failed to get calloc");
  } else {
    logf("calloc 0x%p", stub.calloc);
  }

  stub.free =
      reinterpret_cast<CRTStubs::free_t>(GetProcAddress(crt_mod, "free"));
  if (!stub.free) {
    logf("Failed to get free");
  } else {
    logf("free 0x%p", stub.free);
  }
}

void *__cdecl CRT::recalloc(void *ptr, size_t _Count, size_t _Size) {
  return stub.recalloc(ptr, _Count, _Size);
}

void *__cdecl CRT::calloc(size_t _Count, size_t _Size) {
  return stub.calloc(_Count, _Size);
}

void __cdecl CRT::free(void *_Block) { return stub.free(_Block); }