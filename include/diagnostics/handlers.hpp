#pragma once

#include <windows.h>

namespace Diagnostics {

void startup();
void teardown();
void emit_game_state(const char *phase);
bool should_suppress_game_log(const char *message);
void emit_game_log(const char *message);
void emit_exception_event(const char *type, EXCEPTION_POINTERS *info);

struct PnTcpFrameHeader {
  unsigned payload_len;
  unsigned opcode;
  unsigned rmi_id;
  unsigned body_len;
  unsigned char has_rmi;
};

void emit_proudnet_tcp(DWORD thread_id, unsigned port, const char *direction,
                       unsigned long long sock, size_t chunk_len,
                       const PnTcpFrameHeader *frames, size_t frame_count,
                       size_t incomplete_tail);
void emit_proudnet_tcp_connect(DWORD thread_id, unsigned long long sock,
                               const char *addr, unsigned port);

} // namespace Diagnostics

extern "C" void __cdecl diagnostics_startup();