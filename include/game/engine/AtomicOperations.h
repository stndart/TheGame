#pragma once

#include <cstdint>

/// atomically increment a variable
inline int32_t AtomicIncrement(int32_t &value);

/// atomically decrement a variable
inline int32_t AtomicDecrement(int32_t &value);

/// atomically increment a variable
inline uint32_t AtomicIncrement(uint32_t &value);

/// atomically decrement a variable
inline uint32_t AtomicDecrement(uint32_t &value);

#include "game/engine/AtomicOperations.inl"