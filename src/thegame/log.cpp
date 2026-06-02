#include "thegame/log.hpp"

#include "diagnostics/handlers.hpp"
#include "thegame/config.hpp"
#include "thegame/paths.hpp"

#include <cstring>

#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <windows.h>

namespace thegame {

std::ofstream log_file;
std::ofstream netlog_file;
std::ofstream proudlog_file;
bool console_created = false;

void create_console() {
  if (cfg.no_console || console_created)
    return;

  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);
  console_created = true;
}

void log_message(const char *message) {
  if (!cfg.no_console) {
    if (!console_created)
      create_console();
    printf("%s\n", message);
  }

  if (!log_file.is_open())
    log_file.open(main_log_path(), std::ios::app);
  if (log_file.is_open())
    log_file << message << std::endl;
}

void logf(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  va_end(args);
  log_message(buffer);

  if (cfg.pipes)
    Diagnostics::emit_game_log(buffer);
}

void exceptionf(const char *type, EXCEPTION_POINTERS *info, const char *format,
                ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  va_end(args);
  if (buffer[0] != '\0')
    log_message(buffer);
  if (info)
    Diagnostics::emit_exception_event(type, info, buffer);
  else
    Diagnostics::emit_custom_exception(buffer);
}

// the same as logf, but doesn't print to console
void eventf(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  va_end(args);
  if (cfg.pipes)
    Diagnostics::emit_game_log(buffer);
}

void stagef(const char *stage) {
  char buffer[512];
  sprintf_s(buffer, "[stage] %s", stage);
  log_message(buffer);
  if (cfg.pipes)
    Diagnostics::emit_game_stage(stage);
}

void log_boot_paths() {
  logf("thegame logs: main=%s net=%s proud=%s", main_log_path().c_str(),
       net_log_path().c_str(), proud_log_path().c_str());
  logf("thegame flags: hooks=%d entrypoint=%d veh=%d pipes=%d",
       cfg.disable_hooks ? 0 : 1, cfg.disable_entrypoint_hook ? 0 : 1,
       cfg.disable_veh ? 0 : 1, cfg.pipes ? 1 : 0);
  logf("thegame log flags: no_net=%d silent_net=%d no_proud=%d silent_proud=%d "
       "silent_keepalive=%d",
       cfg.no_network_logs ? 1 : 0, cfg.silent_network ? 1 : 0,
       cfg.no_proud_logs ? 1 : 0, cfg.silent_proud ? 1 : 0,
       cfg.silent_keepalive ? 1 : 0);
}

void ensure_netlog_open() {
  if (netlog_file.is_open())
    return;
  netlog_file.open(net_log_path(), std::ios::app);
  netlog_file << "log start\n";
}

void logns(int socket, const char *addr, int port) {
  if (cfg.no_network_logs)
    return;

  ensure_netlog_open();

  netlog_file << "Connecting socket " << socket << " to " << addr << ":"
              << std::dec << port << "\n";
  netlog_file.flush();

  if (!cfg.silent_network)
    logf("[net] connect socket %d to %s:%d", socket, addr, port);
}

void lognf(int socket, const char *format, ...) {
  if (cfg.no_network_logs)
    return;

  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  va_end(args);

  ensure_netlog_open();

  netlog_file << "Error with socket " << socket << ": " << buffer << "\n";
  netlog_file.flush();

  if (!cfg.silent_network)
    logf("[net] error socket %d: %s", socket, buffer);
}

bool is_keepalive_packet(size_t len) { return len <= 8; }

void logn(int socket, size_t len, char *data, bool in) {
  if (cfg.no_network_logs)
    return;
  if (cfg.silent_keepalive && is_keepalive_packet(len))
    return;

  ensure_netlog_open();

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
  netlog_file.flush();

  if (!cfg.silent_network) {
    logf("[net] %s socket %d len=%u", in ? "rx" : "tx", socket,
         static_cast<unsigned>(len));
  }
}

void ensure_proudlog_open() {
  if (cfg.no_proud_logs || proudlog_file.is_open())
    return;
  proudlog_file.open(proud_log_path(), std::ios::app);
  if (proudlog_file.is_open())
    proudlog_file << "# proudnet-tcp headers (tid port dir chunk frames)\n";
}

void logpns(int socket, const char *addr, unsigned short port) {
  if (cfg.no_proud_logs)
    return;

  ensure_proudlog_open();
  if (!proudlog_file.is_open())
    return;

  const DWORD tid = GetCurrentThreadId();
  proudlog_file << "tid=" << tid << " event=connect port=" << port
                << " sock=" << socket << " addr=" << (addr ? addr : "?")
                << "\n";
  proudlog_file.flush();

  if (!cfg.silent_proud)
    logf("[proud] connect socket %d port=%u addr=%s", socket,
         static_cast<unsigned>(port), addr ? addr : "?");

  if (cfg.pipes) {
    Diagnostics::emit_proudnet_tcp_connect(
        tid, static_cast<unsigned long long>(socket), addr, port);
  }
}

void logpnf(int socket, const char *format, ...) {
  if (cfg.no_proud_logs)
    return;

  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsprintf_s(buffer, format, args);
  va_end(args);

  ensure_proudlog_open();
  if (!proudlog_file.is_open())
    return;

  proudlog_file << "Error with socket " << socket << ": " << buffer << "\n";
  proudlog_file.flush();

  if (!cfg.silent_proud)
    logf("[proud] error socket %d: %s", socket, buffer);
}

void logpln(const char *line) {
  if (cfg.no_proud_logs || !line || !line[0])
    return;

  ensure_proudlog_open();
  if (!proudlog_file.is_open())
    return;

  proudlog_file << line << "\n";
  proudlog_file.flush();

  if (!cfg.silent_proud)
    logf("[proud] %s", line);
}

void logpln_silent(const char *line) {
  if (cfg.no_proud_logs || !line || !line[0])
    return;

  ensure_proudlog_open();
  if (!proudlog_file.is_open())
    return;

  proudlog_file << line << "\n";
  proudlog_file.flush();
}

void close_logs() {
  if (log_file.is_open())
    log_file.close();
  if (netlog_file.is_open())
    netlog_file.close();
  if (proudlog_file.is_open())
    proudlog_file.close();
}

} // namespace thegame
