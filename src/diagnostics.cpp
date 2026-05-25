#include "diagnostics.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr const char *kPipeName = R"(\\.\pipe\thegame-diagnostics)";
constexpr DWORD kDbgPrintExceptionC = 0x40010006;
constexpr DWORD kDbgPrintExceptionWideC = 0x4001000A;
constexpr DWORD kRpcServerUnavailable = 0x000006BA;
HANDLE g_pipe = INVALID_HANDLE_VALUE;
PVOID g_veh_handle = nullptr;
CRITICAL_SECTION g_pipe_lock;
bool g_lock_initialized = false;
volatile LONG g_started = 0;
volatile LONG g_handling_exception = 0;

void ensure_lock() {
  if (!g_lock_initialized) {
    InitializeCriticalSection(&g_pipe_lock);
    g_lock_initialized = true;
  }
}

void json_escape(char *dst, size_t dst_size, const char *src) {
  if (!dst_size)
    return;

  size_t pos = 0;
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(src);
       p && *p && pos + 1 < dst_size; ++p) {
    if (*p == '"' || *p == '\\') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = static_cast<char>(*p);
    } else if (*p == '\n') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = 'n';
    } else if (*p == '\r') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = 'r';
    } else if (*p == '\t') {
      if (pos + 2 >= dst_size)
        break;
      dst[pos++] = '\\';
      dst[pos++] = 't';
    } else if (*p < 0x20) {
      if (pos + 6 >= dst_size)
        break;
      int written = _snprintf_s(dst + pos, dst_size - pos, _TRUNCATE,
                                "\\u%04x", static_cast<unsigned>(*p));
      if (written < 0)
        break;
      pos += static_cast<size_t>(written);
    } else {
      dst[pos++] = static_cast<char>(*p);
    }
  }
  dst[pos] = '\0';
}

bool connect_pipe() {
  if (g_pipe != INVALID_HANDLE_VALUE)
    return true;

  g_pipe = CreateFileA(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
  return g_pipe != INVALID_HANDLE_VALUE;
}

void write_line_unlocked(const char *line) {
  if (!line || !connect_pipe())
    return;

  DWORD written = 0;
  WriteFile(g_pipe, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
  WriteFile(g_pipe, "\n", 1, &written, nullptr);
}

bool is_ignored_first_chance_exception(DWORD code) {
  return code == kDbgPrintExceptionC || code == kDbgPrintExceptionWideC ||
         code == kRpcServerUnavailable;
}

void emit_exception_event(const char *type, EXCEPTION_POINTERS *info) {
  char line[512];
  EXCEPTION_RECORD *record = info->ExceptionRecord;
  CONTEXT *context = info->ContextRecord;
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"%s\",\"pid\":%lu,\"code\":\"0x%08lX\","
              "\"address\":\"0x%p\",\"eip\":\"0x%08lX\",\"esp\":\"0x%08lX\","
              "\"ebp\":\"0x%08lX\"}",
              type, GetCurrentProcessId(), record->ExceptionCode,
              record->ExceptionAddress, context->Eip, context->Esp,
              context->Ebp);

  // Avoid taking the normal log lock here: the fault may have happened while
  // that lock was held, and this is best-effort crash telemetry.
  write_line_unlocked(line);
}

LONG WINAPI vectored_exception_handler(EXCEPTION_POINTERS *info) {
  if (is_ignored_first_chance_exception(info->ExceptionRecord->ExceptionCode))
    return EXCEPTION_CONTINUE_SEARCH;

  if (InterlockedExchange(&g_handling_exception, 1) != 0)
    return EXCEPTION_CONTINUE_SEARCH;

  emit_exception_event("first_chance_exception", info);
  InterlockedExchange(&g_handling_exception, 0);

  return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS *info) {
  if (InterlockedExchange(&g_handling_exception, 1) == 0) {
    emit_exception_event("fatal_exception", info);
    InterlockedExchange(&g_handling_exception, 0);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

namespace Diagnostics {

void start_from_entrypoint() {
  if (InterlockedExchange(&g_started, 1) != 0)
    return;

  ensure_lock();
  g_veh_handle = AddVectoredExceptionHandler(1, vectored_exception_handler);
  SetUnhandledExceptionFilter(unhandled_exception_handler);

  char line[160];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"hello\",\"pid\":%lu,\"version\":1}",
              GetCurrentProcessId());
  emit_event_line(line);
}

void shutdown() {
  if (g_veh_handle) {
    RemoveVectoredExceptionHandler(g_veh_handle);
    g_veh_handle = nullptr;
  }
  if (g_pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
  }
}

void emit_log(const char *message) {
  if (InterlockedCompareExchange(&g_started, 1, 1) == 0)
    return;

  char escaped[900];
  json_escape(escaped, sizeof(escaped), message ? message : "");

  char line[1024];
  _snprintf_s(line, sizeof(line), _TRUNCATE,
              "{\"type\":\"log\",\"pid\":%lu,\"message\":\"%s\"}",
              GetCurrentProcessId(), escaped);
  emit_event_line(line);
}

void emit_event_line(const char *line) {
  if (!line)
    return;

  ensure_lock();
  EnterCriticalSection(&g_pipe_lock);
  write_line_unlocked(line);
  LeaveCriticalSection(&g_pipe_lock);
}

} // namespace Diagnostics

extern "C" void __cdecl diagnostics_start_from_entrypoint() {
  Diagnostics::start_from_entrypoint();
}
