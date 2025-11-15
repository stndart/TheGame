#include "console.h"
#include <windows.h>

#include "game/engine/rcstring.h"

void create_console() {
  if (console_created)
    return;

  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);

  console_created = true;
}

void log_message(const char *message) {
  // Create console on first log
  if (!console_created) {
    create_console();
  }

  printf("%s\n", message);
  OutputDebugStringA(message);

  // Write to file (auto-flushes with endl)
  if (!log_file.is_open()) {
    log_file.open("logs.txt", std::ios::app);
  }
  log_file << message << std::endl;
}

void logf(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  va_end(args);

  log_message(buffer);
}

void log_string_structure(const RefString *str, const char *label) {
  if (str == nullptr) {
    logf("%s: NULL", label);
    return;
  }

  if (str->m_kHandle->m_data == nullstr) {
    logf("%s: nullstr", label);
    return;
  }

  RefString::StringHeader *header =
      RefString::GetRealBufferStart(str->m_kHandle);

  int capacity = 0;
  size_t ref_cnt = 0;
  if (header) {
    capacity = header->m_cbBufferSize;
    ref_cnt = header->m_RefCount;
  }

  logf("%s: at %p, capacity=%d, refcount=%d, data='%s'", label, str, capacity,
       ref_cnt, str->m_kHandle->m_data);
}