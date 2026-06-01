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

bool Diagnostics::format_custom_message(char *line, size_t line_size,
                                        const char *type, const char *message) {
  if (!line || line_size == 0 || !type || !message)
    return false;

  _snprintf_s(line, line_size, _TRUNCATE,
              "{\"type\":\"%s\",\"pid\":%lu,\"tid\":%lu,\"message\":\"%s\"}",
              type, GetCurrentProcessId(), GetCurrentThreadId(), message);
  return true;
}

bool Diagnostics::format_exception_event(char *line, size_t line_size,
                                         const char *type,
                                         EXCEPTION_POINTERS *info) {
  if (!line || line_size == 0 || !type || !info || !info->ExceptionRecord ||
      !info->ContextRecord)
    return false;

  EXCEPTION_RECORD *record = info->ExceptionRecord;
  CONTEXT *context = info->ContextRecord;

  // For access violations ExceptionInformation[0] is the access type
  // (0=read, 1=write, 8=DEP/execute) and [1] is the faulting data address.
  const char *access = "n/a";
  unsigned long fault_addr = 0;
  if (record->ExceptionCode == 0xC0000005 && record->NumberParameters >= 2) {
    const ULONG_PTR kind = record->ExceptionInformation[0];
    access = (kind == 0)   ? "read"
             : (kind == 1) ? "write"
             : (kind == 8) ? "exec"
                           : "?";
    fault_addr = static_cast<unsigned long>(record->ExceptionInformation[1]);
  }

  _snprintf_s(
      line, line_size, _TRUNCATE,
      "{\"type\":\"%s\",\"pid\":%lu,\"tid\":%lu,\"code\":\"0x%08lX\","
      "\"flags\":\"0x%08lX\",\"access\":\"%s\",\"fault_addr\":\"0x%08lX\","
      "\"address\":\"0x%p\",\"eip\":\"0x%08lX\",\"esp\":\"0x%08lX\","
      "\"ebp\":\"0x%08lX\",\"eax\":\"0x%08lX\",\"ebx\":\"0x%08lX\","
      "\"ecx\":\"0x%08lX\",\"edx\":\"0x%08lX\",\"esi\":\"0x%08lX\","
      "\"edi\":\"0x%08lX\"}",
      type, GetCurrentProcessId(), GetCurrentThreadId(), record->ExceptionCode,
      record->ExceptionFlags, access, fault_addr, record->ExceptionAddress,
      context->Eip, context->Esp, context->Ebp, context->Eax, context->Ebx,
      context->Ecx, context->Edx, context->Esi, context->Edi);
  return true;
}

void NamedPipe::emit_exception_event(const char *type,
                                     EXCEPTION_POINTERS *info) {
  char line[1024];
  if (!format_exception_event(line, sizeof(line), type, info))
    return;

  // Avoid taking the normal log lock here: the fault may have happened while
  // that lock was held, and this is best-effort crash telemetry.
  write_line_unlocked(line);
}