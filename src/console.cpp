#include "console.h"
#include <windows.h>

#include <iomanip>

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

void logns(int socket, const char *addr, int port) {
  if (!netlog_file.is_open()) {
    netlog_file.open("netlogs.txt", std::ios::app);
    netlog_file << "log start\n";
  }

  netlog_file << "Connecting socket " << socket << " to " << addr << ":"
              << std::dec << port << "\n";
}

void logn(int socket, size_t len, char *data, bool in) {
  if (!netlog_file.is_open()) {
    netlog_file.open("netlogs.txt", std::ios::app);
    netlog_file << "log start\n";
  }

  if (in)
    netlog_file << socket << " < ";
  else
    netlog_file << socket << " > ";

  for (size_t i = 0; i < len; ++i) {
    netlog_file << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned int>(
                       static_cast<unsigned char>(data[i]));
  }
  netlog_file << std::endl;
}