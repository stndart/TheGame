#include "console.h"
#include <windows.h>

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