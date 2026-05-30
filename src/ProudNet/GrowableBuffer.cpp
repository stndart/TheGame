#include "ProudNet/GrowableBuffer.hpp"

#include "game/engine/MemoryDefines.h"

#include <cstring>
#include <new>
#include <stdexcept>

#include <windows.h>

namespace Proud {
namespace growable {

void *alloc_bytes(void * /*allocator*/, std::size_t bytes) {
  return EE_HEAPALLOC(bytes);
}

void *realloc_bytes(void * /*allocator*/, void *block, std::size_t bytes) {
  if (!block)
    return EE_HEAPALLOC(bytes);
  return EE_HEAPREALLOC(block, bytes);
}

void free_bytes(void * /*allocator*/, void *block) {
  if (!block)
    return;
  EE_HEAPFREE(block);
}

[[noreturn]] void throw_negative_size() {
  throw std::length_error("Proud::GrowableBuffer: negative size");
}

[[noreturn]] void throw_send_buffer_count_out_of_range() {
  throw std::out_of_range("32bit integer out of range");
}

[[noreturn]] void throw_proud_oom() { throw std::bad_alloc(); }

volatile LONG g_wsa_interrupt_retry_count = 0;

void note_wsa_interrupt_retry() {
  InterlockedIncrement(&g_wsa_interrupt_retry_count);
}

int calc_capacity(const Proud::GrowableBuffer &buf, int need) {
  const int size = buf.size_;
  const int mode = buf.growth_mode_;
  const int min_cap = buf.min_capacity_;
  int slack = 0;
  if (mode == 0) {
    slack = size / 8;
    if (slack < 1024) {
      if (slack <= 4)
        slack = 4;
    } else {
      slack = 1024;
    }
  } else if (mode == 1) {
    slack = size / 8;
    if (slack <= 64)
      slack = 64;
  } else if (mode == 2) {
    slack = 0;
  } else {
    throw_negative_size();
  }

  int cap = need + slack;
  if (min_cap > cap)
    cap = min_cap;
  return cap;
}

void ensure_size(Proud::GrowableBuffer &buf, int new_size) {
  if (new_size < 0)
    throw_negative_size();
  if (new_size == buf.size_)
    return;

  if (new_size <= buf.capacity_) {
    buf.size_ = new_size;
    return;
  }

  const int new_cap = calc_capacity(buf, new_size);
  const std::size_t bytes = static_cast<std::size_t>(new_cap);
  void *block = nullptr;

  if (!buf.capacity_) {
    block = alloc_bytes(buf.allocator_, bytes);
  } else {
    block = realloc_bytes(buf.allocator_, buf.data_, bytes);
  }

  if (!block)
    throw_proud_oom();

  buf.data_ = static_cast<char *>(block);
  buf.capacity_ = new_cap;
  buf.size_ = new_size;
}

void ensure_capacity_copy(Proud::RecvBuffer &buf, int new_size) {
  if (new_size < 0)
    throw_negative_size();
  if (new_size == buf.size_)
    return;

  if (new_size > buf.capacity_) {
    const int new_cap = calc_capacity(buf, new_size);
    const std::size_t bytes = static_cast<std::size_t>(new_cap);

    if (!buf.capacity_) {
      char *block = static_cast<char *>(alloc_bytes(buf.allocator_, bytes));
      if (!block)
        throw_proud_oom();
      buf.capacity_ = new_cap;
      buf.size_ = new_size;
      buf.data_ = block;
      return;
    }

    char *const old = buf.data_;
    char *block = static_cast<char *>(alloc_bytes(buf.allocator_, bytes));
    if (!block)
      throw_proud_oom();

    if (buf.size_ > 0) {
      for (int i = 0; i < buf.size_; ++i)
        block[i] = old[i];
    }
    free_bytes(buf.allocator_, old);
    buf.data_ = block;
    buf.capacity_ = new_cap;
  }

  buf.size_ = new_size;
}

} // namespace growable
} // namespace Proud
