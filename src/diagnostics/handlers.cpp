#include <cstdio> // IWYU pragma: keep

#include "diagnostics/handlers.hpp"
#include "diagnostics/namedpipe.hpp"
#include "helpers/strhelp.h"

using namespace Diagnostics;

namespace {

constexpr DWORD kDbgPrintExceptionC = 0x40010006;
constexpr DWORD kDbgPrintExceptionWideC = 0x4001000A;

// Re-entrancy guard: emit_game_log / pipe I/O must not recurse through this VEH.
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
  g_diagnostics_started = true;

  if (!g_veh_handle)
    g_veh_handle = AddVectoredExceptionHandler(1, vectored_exception_handler);
  SetUnhandledExceptionFilter(unhandled_exception_handler);

  g_pipe = connect_pipe();
  if (!g_pipe)
    return;

  char line[160];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"hello\",\"pid\":%lu,\"version\":1}",
              GetCurrentProcessId());

  g_pipe->write_line_locked(line);
  emit_game_state("started");
}

void teardown() {
  g_diagnostics_started = false;

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

extern "C" void __cdecl diagnostics_startup() { startup(); }