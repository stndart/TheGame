#pragma once

#include <cstddef>

#include "game/net/pn_layout.hpp"

namespace pn {
namespace growable {

int calc_capacity(const PNGrowableBuffer &buf, int need);
void ensure_size(PNGrowableBuffer &buf, int new_size);
void ensure_capacity_copy(PNRecvBuffer &buf, int new_size);

// allocator argument ignored - always process heap (matches game w_heap_alloc
// path).
void *alloc_bytes(void *allocator, std::size_t bytes);
void *realloc_bytes(void *allocator, void *block, std::size_t bytes);
void free_bytes(void *allocator, void *block);

[[noreturn]] void throw_negative_size();
[[noreturn]] void throw_send_buffer_count_out_of_range();
[[noreturn]] void throw_proud_oom();

// GAME.exe @ 0x180DF00 - shared WSAEINTR retry tally (TCPSocket +
// PNFastSocket).
void note_wsa_interrupt_retry();

} // namespace growable
} // namespace pn
