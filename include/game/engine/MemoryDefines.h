#pragma once

#define EE_MEM_DEALLOC_SIZE_DEFAULT (size_t)-1

void _Free(void *pvMemory, size_t stSizeInBytes = EE_MEM_DEALLOC_SIZE_DEFAULT);

void *_Malloc(size_t stSizeInBytes);

void *_Realloc(void *memblock, size_t stSizeInBytes);

bool _HeapFree(void *lpMem);

#define EE_FREE(p) _Free(p)

#define EE_ALLOC(T, count) ((T *)_Malloc(sizeof(T) * (count)))

#define EE_REALLOC(memblock, size) (_Realloc(memblock, size))

#define EE_HEAPFREE(lpMem) (_HeapFree(lpMem))
