#pragma once

#include <windows.h>

namespace Diagnostics {

constexpr const char *kPipeName = R"(\\.\pipe\thegame-diagnostics)";

class NamedPipe {
private:
  HANDLE g_pipe = INVALID_HANDLE_VALUE;
  CRITICAL_SECTION g_pipe_lock;
  bool g_lock_initialized = false;

public:
  NamedPipe(const char *pipe_name = kPipeName);
  ~NamedPipe();

  bool connect_pipe();
  void write_line_unlocked(const char *line);
  void write_line_locked(const char *line);

  void emit_exception_event(const char *type, EXCEPTION_POINTERS *info);
};

}; // namespace Diagnostics