#include "diagnostics/handlers.hpp"

#include "helpers/strhelp.h"

#include "RMI/Nav.hpp"
#include "diagnostics/exceptions.hpp"
#include "diagnostics/handler_pipe.hpp"
#include "diagnostics/namedpipe.hpp"
#include "thegame/config.hpp"

using namespace Diagnostics;

namespace Diagnostics {

bool g_diagnostics_started = false;
LONG g_handling_exception = 0;
char g_last_game_stage[32] = "";

NamedPipe *g_pipe = nullptr;

NamedPipe *connect_pipe() {
  if (!g_pipe) {
    g_pipe = new NamedPipe(kPipeName);
  }
  if (!g_pipe || !g_pipe->connect_pipe())
    return nullptr;
  return g_pipe;
}

// Is meant to be called after entrypoint.
// Sets VEH, opens pipes and sends hello message
void startup() {
  if (g_diagnostics_started)
    return;

  if (!thegame::cfg.disable_veh) {
    if (!g_veh_handle)
      g_veh_handle = AddVectoredExceptionHandler(1, vectored_exception_handler);
    SetUnhandledExceptionFilter(unhandled_exception_handler);
  }

  if (thegame::cfg.minidump_enabled && !g_minidump_fn) {
    if (HMODULE dbghelp = LoadLibraryA("dbghelp.dll"))
      g_minidump_fn = reinterpret_cast<MiniDumpWriteDump_t>(
          GetProcAddress(dbghelp, "MiniDumpWriteDump"));
  }
  g_av_park = thegame::cfg.disable_park_thread ? 0 : 1;

  if (thegame::cfg.pipes) {
    // wait 5s for pipe to connect
    for (int i = 0; i < 50; ++i) {
      g_pipe = connect_pipe();
      if (g_pipe)
        break;
      Sleep(100);
    }
    if (!g_pipe)
      return;
  }

  g_diagnostics_started = true;

  emit_message("hello", "version=1");
  emit_game_stage("started");

  HandlerPipe::start();
  Rmi::NavStartup();
}

bool started() { return g_diagnostics_started; }

void teardown() {
  g_diagnostics_started = false;
  HandlerPipe::stop();
  Rmi::NavTeardown();

  if (g_veh_handle) {
    RemoveVectoredExceptionHandler(g_veh_handle);
    g_veh_handle = nullptr;
  }

  if (g_pipe) {
    delete g_pipe;
    g_pipe = nullptr;
  }
}

bool write_emit_prologue(const char *type, char *line, size_t size,
                         const char *format, ...) {
  va_list args;
  va_start(args, format);
  int pos = _snprintf_s(line, size, _TRUNCATE,
                        "{\"type\":\"%s\",\"pid\":%lu,\"tid\":%lu", type,
                        GetCurrentProcessId(), GetCurrentThreadId());
  if (pos >= 0)
    pos = _vsnprintf_s(line + pos, size - pos, _TRUNCATE, format, args);
  va_end(args);
  return pos >= 0;
}

void emit_message(const char *type, const char *message) {
  if (!thegame::cfg.pipes)
    return;
  g_pipe = connect_pipe();
  if (!g_pipe || !message)
    return;

  char escaped[1024];
  json_escape(escaped, sizeof(escaped), message);

  char line[2048];
  if (write_emit_prologue(type, line, sizeof(line), ",\"message\":\"%s\"}",
                          escaped)) {
    g_pipe->write_line_locked(line);
  }
}

void emit_game_stage(const char *stage) {
  if (!thegame::cfg.pipes)
    return;
  g_pipe = connect_pipe();
  if (!g_pipe || !stage)
    return;

  if (strncmp(g_last_game_stage, stage, sizeof(g_last_game_stage)) == 0)
    return;

  _snprintf_s(g_last_game_stage, sizeof(g_last_game_stage), _TRUNCATE, "%s",
              stage);

  char escaped[256];
  json_escape(escaped, sizeof(escaped), stage);

  char line[512];
  if (write_emit_prologue("game_stage", line, sizeof(line),
                          ",\"stage\":\"%s\"}", escaped)) {
    g_pipe->write_line_locked(line);
  }
}

void emit_game_log(const char *line) { emit_message("log", line); }

void emit_custom_exception(const char *message) {
  emit_message("exception", message);
}

void emit_exception_event(const char *type, EXCEPTION_POINTERS *info) {
  if (InterlockedExchange(&g_handling_exception, 1) != 0)
    return;

  if (thegame::cfg.pipes) {
    g_pipe = connect_pipe();
    if (g_pipe)
      g_pipe->emit_exception_event(type, info);
  }

  InterlockedExchange(&g_handling_exception, 0);
}

// TODO: move and redo

void emit_proudnet_tcp(DWORD thread_id, unsigned port, const char *direction,
                       unsigned long long sock, size_t chunk_len,
                       const PnTcpFrameHeader *frames, size_t frame_count,
                       size_t incomplete_tail) {
  if (!thegame::cfg.pipes)
    return;
  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char line[2048];
  int pos = _snprintf_s(
      line, sizeof(line), _TRUNCATE,
      "{\"type\":\"proudnet-tcp\",\"pid\":%lu,\"thread_id\":%lu,\"port\":%u,"
      "\"dir\":\"%s\",\"sock\":\"0x%llX\",\"chunk_len\":%zu,\"incomplete\":%zu,"
      "\"frames\":[",
      GetCurrentProcessId(), thread_id, static_cast<unsigned>(port),
      direction ? direction : "", static_cast<unsigned long long>(sock),
      chunk_len, incomplete_tail);

  if (pos < 0)
    return;

  for (size_t i = 0; i < frame_count && pos > 0; ++i) {
    const PnTcpFrameHeader &f = frames[i];
    const int n = _snprintf_s(
        line + pos, sizeof(line) - static_cast<size_t>(pos), _TRUNCATE,
        "%s{\"payload_len\":%u,\"opcode\":%u,\"rmi_id\":%u,\"body_len\":%u,"
        "\"has_rmi\":%s}",
        (i ? "," : ""), f.payload_len, f.opcode, f.rmi_id, f.body_len,
        f.has_rmi ? "true" : "false");
    if (n < 0)
      break;
    pos += n;
    if (static_cast<size_t>(pos) >= sizeof(line) - 4)
      break;
  }

  if (pos > 0 && static_cast<size_t>(pos) < sizeof(line) - 2)
    _snprintf_s(line + pos, sizeof(line) - static_cast<size_t>(pos), _TRUNCATE,
                "]}");

  g_pipe->write_line_locked(line);
}

void emit_proudnet_tcp_connect(DWORD thread_id, unsigned long long sock,
                               const char *addr, unsigned port) {
  if (!thegame::cfg.pipes)
    return;
  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char escaped[128];
  json_escape(escaped, sizeof(escaped), addr ? addr : "");

  char line[384];
  _snprintf_s(
      line, sizeof(line), _TRUNCATE,
      "{\"type\":\"proudnet-tcp\",\"event\":\"connect\",\"pid\":%lu,"
      "\"thread_id\":%lu,\"port\":%u,\"sock\":\"0x%llX\",\"addr\":\"%s\"}",
      GetCurrentProcessId(), thread_id, static_cast<unsigned>(port),
      static_cast<unsigned long long>(sock), escaped);
  g_pipe->write_line_locked(line);
}

} // namespace Diagnostics