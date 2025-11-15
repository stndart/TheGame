#include <windows.h>

#include <cstdint>
#include <winnt.h>

inline int32_t AtomicIncrement(int32_t &value) {
  return InterlockedIncrement((long *)&value);
}

inline int32_t AtomicDecrement(int32_t &value) {
  return InterlockedDecrement((long *)&value);
}

inline uint32_t AtomicIncrement(uint32_t &value) {
  return InterlockedIncrement((long *)&value);
}

inline uint32_t AtomicDecrement(uint32_t &value) {
  return InterlockedDecrement((long *)&value);
}
