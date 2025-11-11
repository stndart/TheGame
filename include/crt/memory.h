#pragma once
#include <cstddef>

namespace CRT {
struct CRTStubs {
  using recalloc_t = void *(__cdecl *)(void *, size_t, size_t);
  using calloc_t = void *(__cdecl *)(size_t, size_t);
  using free_t = void(__cdecl *)(void *);

  recalloc_t recalloc = nullptr;
  calloc_t calloc = nullptr;
  free_t free = nullptr;
};

extern CRTStubs stub;

void init_CRT();

void *__cdecl recalloc(void *ptr, size_t _Count, size_t _Size);
void *__cdecl calloc(size_t _Count, size_t _Size);
void __cdecl free(void *_Block);

} // namespace CRT