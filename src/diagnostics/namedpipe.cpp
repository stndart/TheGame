#include <cstdio> // IWYU pragma: keep
#include <cstring>

#include "diagnostics/namedpipe.hpp"

using namespace Diagnostics;

NamedPipe::NamedPipe(const char *pipe_name) {
  InitializeCriticalSection(&g_pipe_lock);
  g_lock_initialized = true;

  EnterCriticalSection(&g_pipe_lock);
  g_pipe = CreateFileA(pipe_name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
  LeaveCriticalSection(&g_pipe_lock);
}

NamedPipe::~NamedPipe() {
  EnterCriticalSection(&g_pipe_lock);
  if (g_pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
  }
  LeaveCriticalSection(&g_pipe_lock);
  DeleteCriticalSection(&g_pipe_lock);
}

bool NamedPipe::connect_pipe() {
  if (g_pipe != INVALID_HANDLE_VALUE)
    return true;

  g_pipe = CreateFileA(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
  return g_pipe != INVALID_HANDLE_VALUE;
}

void NamedPipe::write_line_unlocked(const char *line) {
  if (!line || !connect_pipe())
    return;

  DWORD written = 0;
  WriteFile(g_pipe, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
  WriteFile(g_pipe, "\n", 1, &written, nullptr);
}

void NamedPipe::write_line_locked(const char *line) {
  EnterCriticalSection(&g_pipe_lock);
  write_line_unlocked(line);
  LeaveCriticalSection(&g_pipe_lock);
}

void NamedPipe::emit_exception_event(const char *type,
                                     EXCEPTION_POINTERS *info) {
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