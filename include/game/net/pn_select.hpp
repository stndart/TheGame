#pragma once

#include <cstdint>

// Opaque stand-in for sub_D55300 fd-set bundle (`this` in ecx).
class PNSelectContext {
public:
  // sub_D55300 — CFastSocket poll after select().
  char poll(void *fast_socket_ctx, std::uint32_t *out);
};
