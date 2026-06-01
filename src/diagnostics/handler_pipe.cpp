#include "diagnostics/handler_pipe.hpp"

#include "RMI/NavCommands.hpp"
#include "thegame/log.hpp"

#include <cstdio>
#include <cstring>
#include <windows.h>

namespace {

constexpr const char *kHandlerPipeName = R"(\\.\pipe\thegame-handler)";
constexpr DWORD kConnectRetryMs = 100;
constexpr DWORD kReadChunk = 4096;

CRITICAL_SECTION g_pipe_lock;
bool g_lock_init = false;
HANDLE g_pipe = INVALID_HANDLE_VALUE;
HANDLE g_thread = nullptr;
volatile LONG g_running = 0;
volatile LONG g_connected_logged = 0;

char g_read_pending[8192];
std::size_t g_read_pending_len = 0;

bool ensure_lock() {
  if (!g_lock_init) {
    InitializeCriticalSection(&g_pipe_lock);
    g_lock_init = true;
  }
  return true;
}

void close_pipe_locked() {
  if (g_pipe != INVALID_HANDLE_VALUE) {
    CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
  }
  g_read_pending_len = 0;
  InterlockedExchange(&g_connected_logged, 0);
}

bool connect_pipe_locked() {
  if (g_pipe != INVALID_HANDLE_VALUE)
    return true;

  for (int i = 0; i < 300; ++i) {
    g_pipe = CreateFileA(kHandlerPipeName, GENERIC_READ | GENERIC_WRITE, 0,
                         nullptr, OPEN_EXISTING, 0, nullptr);
    if (g_pipe != INVALID_HANDLE_VALUE) {
      DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
      SetNamedPipeHandleState(g_pipe, &mode, nullptr, nullptr);
      if (InterlockedCompareExchange(&g_connected_logged, 1, 0) == 0) {
        thegame::logf("handler: pipe connected");
      }
      return true;
    }
    const DWORD err = GetLastError();
    if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND)
      break;
    LeaveCriticalSection(&g_pipe_lock);
    Sleep(kConnectRetryMs);
    EnterCriticalSection(&g_pipe_lock);
  }
  return false;
}

bool write_response_locked(const char *line) {
  if (!line || !connect_pipe_locked())
    return false;

  DWORD written = 0;
  const DWORD len = static_cast<DWORD>(strlen(line));
  if (!WriteFile(g_pipe, line, len, &written, nullptr) || written != len)
    return false;
  if (!WriteFile(g_pipe, "\n", 1, &written, nullptr) || written != 1)
    return false;
  return true;
}

bool dispatch_line(const char *line) {
  if (!line || !line[0])
    return true;

  if (strcmp(line, "commands") == 0) {
    return write_response_locked(Rmi::NavCommandList());
  }

  if (strcmp(line, "nav_goto_lobby") == 0) {
    Rmi::NavEnqueueCommand(Rmi::NavCmd::GotoLobby);
    return write_response_locked("ok");
  }

  char err[96];
  _snprintf_s(err, sizeof(err), _TRUNCATE, "error: unknown command %s", line);
  return write_response_locked(err);
}

void consume_line(char *line) {
  char *cr = strchr(line, '\r');
  if (cr)
    *cr = '\0';
  dispatch_line(line);
}

bool read_more_locked() {
  if (!connect_pipe_locked())
    return false;

  char chunk[kReadChunk];
  DWORD read = 0;
  while (true) {
    if (!ReadFile(g_pipe, chunk, sizeof(chunk), &read, nullptr)) {
      const DWORD err = GetLastError();
      if (err == ERROR_NO_DATA)
        break;
      if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
        close_pipe_locked();
        return false;
      }
      if (err == ERROR_MORE_DATA) {
        // partial read still valid
      } else {
        close_pipe_locked();
        return false;
      }
    }
    if (read == 0)
      break;

    if (g_read_pending_len + read >= sizeof(g_read_pending)) {
      g_read_pending_len = 0;
      break;
    }
    memcpy(g_read_pending + g_read_pending_len, chunk, read);
    g_read_pending_len += read;
  }

  char *start = g_read_pending;
  char *nl = nullptr;
  while ((nl = strchr(start, '\n')) != nullptr) {
    *nl = '\0';
    consume_line(start);
    start = nl + 1;
  }

  const std::size_t remaining = g_read_pending + g_read_pending_len - start;
  if (start != g_read_pending && remaining > 0)
    memmove(g_read_pending, start, remaining);
  g_read_pending_len = remaining;
  return true;
}

DWORD WINAPI reader_thread(LPVOID) {
  ensure_lock();
  while (InterlockedCompareExchange(&g_running, 0, 0) != 0) {
    EnterCriticalSection(&g_pipe_lock);
    read_more_locked();
    LeaveCriticalSection(&g_pipe_lock);
    Sleep(50);
  }
  return 0;
}

} // namespace

namespace Diagnostics {
namespace HandlerPipe {

void start() {
  if (InterlockedCompareExchange(&g_running, 1, 0) != 0)
    return;
  ensure_lock();
  g_thread = CreateThread(nullptr, 0, reader_thread, nullptr, 0, nullptr);
}

void stop() {
  InterlockedExchange(&g_running, 0);
  if (g_thread) {
    WaitForSingleObject(g_thread, 5000);
    CloseHandle(g_thread);
    g_thread = nullptr;
  }
  if (g_lock_init) {
    EnterCriticalSection(&g_pipe_lock);
    close_pipe_locked();
    LeaveCriticalSection(&g_pipe_lock);
    DeleteCriticalSection(&g_pipe_lock);
    g_lock_init = false;
  }
}

} // namespace HandlerPipe
} // namespace Diagnostics
