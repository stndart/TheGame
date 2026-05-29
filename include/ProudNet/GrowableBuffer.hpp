#pragma once

#include <cstddef>

#include "ProudNet/Layout.hpp"

namespace Proud {
namespace growable {

int calc_capacity(const GrowableBuffer &buf, int need);
void ensure_size(GrowableBuffer &buf, int new_size);
void ensure_capacity_copy(RecvBuffer &buf, int new_size);

// allocator argument ignored - always process heap (matches game w_heap_alloc
// path).
void *alloc_bytes(void *allocator, std::size_t bytes);
void *realloc_bytes(void *allocator, void *block, std::size_t bytes);
void free_bytes(void *allocator, void *block);

[[noreturn]] void throw_negative_size();
[[noreturn]] void throw_send_buffer_count_out_of_range();
[[noreturn]] void throw_proud_oom();

// GAME.exe @ 0x180DF00 - shared WSAEINTR retry tally (TCPSocket +
// Proud::CFastSocket).
void note_wsa_interrupt_retry();

} // namespace growable
} // namespace Proud
