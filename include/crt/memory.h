#pragma once

#include <cstddef>
#include <windows.h>
namespace CRT {
struct CRTStubs {
  using recalloc_t = void *(__cdecl *)(void *, size_t, size_t);
  using calloc_t = void *(__cdecl *)(size_t, size_t);
  using free_t = void(__cdecl *)(void *);
  using get_proc_heap_t = HANDLE(__stdcall *)();
  using heap_free_t = bool(__stdcall *)(HANDLE hHeap, DWORD dwFlags,
                                        LPVOID lpMem);

  recalloc_t recalloc = nullptr;
  calloc_t calloc = nullptr;
  free_t free = nullptr;
  get_proc_heap_t get_proc_heap = nullptr;
  heap_free_t heap_free = nullptr;
};

extern CRTStubs stub;

void init_CRT();

void *__cdecl recalloc(void *ptr, size_t _Count, size_t _Size);
void *__cdecl calloc(size_t _Count, size_t _Size);
void __cdecl free(void *_Block);

HANDLE __stdcall get_proc_heap();
bool __stdcall heap_free(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

} // namespace CRT