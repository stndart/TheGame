#include "game/engine/MemoryDefines.h"
#include "crt/memory.h"

void _Free(void *pvMem, size_t stSizeInBytes) {
  // EE_ASSERT(MemManager::IsInitialized());
  if (pvMem == nullptr)
    return;

  if (stSizeInBytes == 0)
    stSizeInBytes = 1;

  CRT::free(pvMem);
  // MemManager::Get().Deallocate(pvMem, EE_MET_FREE, stSizeInBytes);
}

void *_Malloc(size_t stSizeInBytes) {
  // EE_ASSERT(MemManager::IsInitialized());

  if (stSizeInBytes == 0)
    stSizeInBytes = 1;

  void *pvMem = CRT::calloc(stSizeInBytes, 1);

  // Actually allocate the memory
  // void *pvMem = MemManager::Get().Allocate(
  //     stSizeInBytes, EE_MEM_ALIGNMENT_DEFAULT, kHint, EE_MET_MALLOC);

  // Return the allocated memory advanced by the size of the header
  return pvMem;
}

void *_Realloc(void *pvMem, size_t stSizeInBytes) {
  //   EE_ASSERT(MemManager::IsInitialized());

  if (stSizeInBytes == 0 && pvMem != 0) {
    EE_FREE(pvMem);
    return NULL;
  } else if (pvMem == 0) {
    return _Malloc(stSizeInBytes);
  }

  pvMem = CRT::recalloc(pvMem, stSizeInBytes, 1);

  // Actually reallocate the memory
  //   pvMem = MemManager::Get().Reallocate(
  //       pvMem, stSizeInBytes, EE_MEM_ALIGNMENT_DEFAULT, kHint,
  //       EE_MET_REALLOC, EE_MEM_DEALLOC_SIZE_DEFAULT);

  return pvMem;
}

bool _HeapFree(void *lpMem) {
  HANDLE ProcessHeap = CRT::get_proc_heap();
  return CRT::heap_free(ProcessHeap, 0, lpMem);
}