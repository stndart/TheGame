#include <cstdio> // IWYU pragma: keep
#include <cstring>

#include "diagnostics/handlers.hpp"
#include "diagnostics/handler_pipe.hpp"
#include "diagnostics/namedpipe.hpp"
#include "helpers/strhelp.h"
#include "RMI/Nav.hpp"

using namespace Diagnostics;

namespace {

constexpr DWORD kDbgPrintExceptionC = 0x40010006;
constexpr DWORD kDbgPrintExceptionWideC = 0x4001000A;

// Re-entrancy guard: emit_game_log / pipe I/O must not recurse through this
// VEH.
thread_local bool g_in_vectored_handler = false;

bool is_dbgprint_exception(DWORD code) {
  return code == kDbgPrintExceptionC || code == kDbgPrintExceptionWideC;
}

LONG WINAPI vectored_exception_handler(EXCEPTION_POINTERS *info) {
  if (g_in_vectored_handler)
    return EXCEPTION_CONTINUE_SEARCH;
  g_in_vectored_handler = true;

  EXCEPTION_RECORD *rec = info->ExceptionRecord;
  const DWORD code = rec->ExceptionCode;

  if (code == kDbgPrintExceptionC && rec->NumberParameters >= 2) {
    const char *msg =
        reinterpret_cast<const char *>(rec->ExceptionInformation[1]);
    Diagnostics::emit_game_log(string_to_string_safe(msg).c_str());
    g_in_vectored_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (code == kDbgPrintExceptionWideC && rec->NumberParameters >= 2) {
    const wchar_t *wmsg =
        reinterpret_cast<const wchar_t *>(rec->ExceptionInformation[1]);
    Diagnostics::emit_game_log(wstring_to_string_safe(wmsg).c_str());
    g_in_vectored_handler = false;
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (!is_dbgprint_exception(code))
    Diagnostics::emit_exception_event("exception", info);

  g_in_vectored_handler = false;
  return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *info) {
  Diagnostics::emit_exception_event("fatal_exception", info);
  return EXCEPTION_CONTINUE_SEARCH;
}

}; // namespace

namespace Diagnostics {

bool should_suppress_game_log(const char *message) {
  if (!message)
    return false;
  if (std::strstr(message, "Error 7") != nullptr &&
      std::strstr(message, "StorageSystem::Registry::CNativeValue::Write") !=
          nullptr)
    return true;
  if (std::strstr(message, "Error 20") != nullptr &&
      std::strstr(message, "AVolute::GetProductInfoT") != nullptr)
    return true;
  return false;
}

PVOID g_veh_handle = nullptr;
LONG g_handling_exception = 0;
bool g_diagnostics_started = false;
char g_last_game_phase[32] = "";

NamedPipe *g_pipe = nullptr;

NamedPipe *connect_pipe() {
  if (!g_pipe) {
    g_pipe = new NamedPipe(kPipeName);
  }
  if (!g_pipe || !g_pipe->connect_pipe())
    return nullptr;
  return g_pipe;
}

void startup() {
  if (g_diagnostics_started)
    return;

  if (!g_veh_handle)
    g_veh_handle = AddVectoredExceptionHandler(1, vectored_exception_handler);
  SetUnhandledExceptionFilter(unhandled_exception_handler);

  for (int i = 0; i < 300; ++i) {
    g_pipe = connect_pipe();
    if (g_pipe)
      break;
    Sleep(100);
  }
  if (!g_pipe)
    return;

  g_diagnostics_started = true;

  char line[160];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"hello\",\"pid\":%lu,\"version\":1}",
              GetCurrentProcessId());

  g_pipe->write_line_locked(line);
  emit_game_state("started");

  HandlerPipe::start();
  Rmi::NavLogStartup();
}

bool started() { return g_diagnostics_started; }

void teardown() {
  g_diagnostics_started = false;
  HandlerPipe::stop();

  if (g_veh_handle) {
    RemoveVectoredExceptionHandler(g_veh_handle);
    g_veh_handle = nullptr;
  }

  if (g_pipe) {
    delete g_pipe;
    g_pipe = nullptr;
  }
}

void emit_game_state(const char *phase) {
  g_pipe = connect_pipe();
  if (!g_pipe || !phase)
    return;

  if (strncmp(g_last_game_phase, phase, sizeof(g_last_game_phase)) == 0)
    return;

  _snprintf_s(g_last_game_phase, sizeof(g_last_game_phase), _TRUNCATE, "%s",
              phase);

  char escaped[64];
  json_escape(escaped, sizeof(escaped), phase);

  char line[256];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"game_state\",\"pid\":%lu,\"phase\":\"%s\"}",
              GetCurrentProcessId(), escaped);
  g_pipe->write_line_locked(line);
}

void emit_game_log(const char *message) {
  if (should_suppress_game_log(message))
    return;

  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char escaped[900];
  json_escape(escaped, sizeof(escaped), message ? message : "");

  char line[1024];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"log\",\"pid\":%lu,\"message\":\"%s\"}",
              GetCurrentProcessId(), escaped);
  g_pipe->write_line_locked(line);
}

void emit_proudnet_tcp(DWORD thread_id, unsigned port, const char *direction,
                       unsigned long long sock, size_t chunk_len,
                       const PnTcpFrameHeader *frames, size_t frame_count,
                       size_t incomplete_tail) {
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

void emit_exception_event(const char *type, EXCEPTION_POINTERS *info) {
  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  if (InterlockedExchange(&g_handling_exception, 1) != 0)
    return;

  g_pipe->emit_exception_event(type, info);

  InterlockedExchange(&g_handling_exception, 0);
}

} // namespace Diagnostics
