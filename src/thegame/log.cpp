#include "thegame/log.hpp"

#include "diagnostics/handlers.hpp"
#include "thegame/config.hpp"
#include "thegame/paths.hpp"

#include <cstring>

#include <cstdio>
#include <windows.h>

#include <fmt/base.h>

namespace thegame {

fmt::rgb color_from_kind(ColorKind kind) {
  switch (kind) {
  case Seen:
    return fmt::rgb(64, 192, 64);
  case Unknown:
    return fmt::rgb(192, 64, 64);
  case Interesting:
    return fmt::rgb(192, 192, 64);
  case Milestone:
    return fmt::rgb(128, 192, 192);
  case White:
  default:
    return fmt::color::white;
  }
}

FILE *log_file = nullptr;
FILE *netlog_file = nullptr;
FILE *proudlog_file = nullptr;
bool console_created = false;

void create_console() {
  if (cfg.no_console || console_created)
    return;

  AllocConsole();
  freopen("CONOUT$", "w", stdout);
  freopen("CONOUT$", "w", stderr);

  if (!cfg.no_colors) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
      DWORD mode = 0;
      if (GetConsoleMode(hOut, &mode))
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
  }

  console_created = true;
}

void log_message(const LogMessage &message) {
  if (!cfg.no_console) {
    if (!console_created)
      create_console();

    if (!cfg.no_colors)
      fmt::print(fmt::fg(color_from_kind(message.kind)), message.message);
    else
      fmt::print(message.message);
  }

  if (!log_file)
    log_file = fopen(main_log_path().c_str(), "a");
  if (log_file) {
    fmt::println(log_file, message.message);
    std::fflush(log_file);
  }
}

void logf(const LogMessage &message) {
  log_message(message);

  if (cfg.pipes)
    Diagnostics::emit_game_log(message.message.c_str());
}

void logf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = fmt::format(format, args);
  va_end(args);
  logf(LogMessage(message));
}

void exceptionf(const char *type, EXCEPTION_POINTERS *info,
                const LogMessage &message) {
  log_message(message);
  if (info)
    Diagnostics::emit_exception_event(type, info, message.message.c_str());
  else
    Diagnostics::emit_custom_exception(message.message.c_str());
}

void exceptionf(const char *type, EXCEPTION_POINTERS *info, const char *format,
                ...) {
  va_list args;
  va_start(args, format);
  std::string message = fmt::format(format, args);
  va_end(args);

  log_message(LogMessage(Exception, fmt::format("{}: {}", type, message)));
  if (info)
    Diagnostics::emit_exception_event(type, info, message.c_str());
  else
    Diagnostics::emit_custom_exception(message.c_str());
}

// the same as logf, but doesn't print to console
void eventf(const LogMessage &message) {
  if (cfg.pipes)
    Diagnostics::emit_game_log(message.message.c_str());
}

void stagef(const char *stage) {
  LogMessage message(Stage, Milestone, stage);
  log_message(message);
  if (cfg.pipes)
    Diagnostics::emit_game_stage(stage);
}

void log_boot_paths() {
  logf(LogMessage(fmt::format("thegame logs: main={} net={} proud={}",
                              main_log_path(), net_log_path(),
                              proud_log_path())));
  logf(LogMessage(fmt::format(
      "thegame flags: hooks={} entrypoint={} veh={} pipes={}",
      cfg.disable_hooks ? 0 : 1, cfg.disable_entrypoint_hook ? 0 : 1,
      cfg.disable_veh ? 0 : 1, cfg.pipes ? 1 : 0)));

  logf(LogMessage(
      fmt::format("thegame log flags: no_net={} silent_net={} no_proud={}"
                  "silent_proud={} "
                  "silent_keepalive={}",
                  cfg.no_network_logs ? 1 : 0, cfg.silent_network ? 1 : 0,
                  cfg.no_proud_logs ? 1 : 0, cfg.silent_proud ? 1 : 0,
                  cfg.silent_keepalive ? 1 : 0)));
}

void ensure_netlog_open() {
  if (netlog_file)
    return;
  netlog_file = fopen(net_log_path().c_str(), "a");
  if (netlog_file) {
    fmt::println(netlog_file, "log start");
    std::fflush(netlog_file);
  }
}

void logns(int socket, const char *addr, int port) {
  if (cfg.no_network_logs)
    return;

  ensure_netlog_open();

  if (netlog_file) {
    fmt::println(netlog_file, "Connecting socket {} to {}:{}", socket, addr,
                 port);
    std::fflush(netlog_file);
  }

  if (!cfg.silent_network)
    logf(LogMessage(
        LogSource::Net,
        fmt::format("[net] connect socket {} to {}:{}", socket, addr, port)));
}

void lognf(int socket, const LogMessage &message) {
  if (cfg.no_network_logs)
    return;

  if (netlog_file) {
    fmt::println(netlog_file, message.message);
    std::fflush(netlog_file);
  }

  if (!cfg.silent_network)
    logf(message);
}

void lognf(int socket, const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = fmt::format(format, args);
  va_end(args);

  std::string line = fmt::format("socket {} : {}", socket, message);

  lognf(socket, LogMessage(LogSource::Net, line));
}

bool is_keepalive_packet(size_t len) { return len <= 8; }

void logn(int socket, size_t len, char *data, bool in) {
  if (cfg.no_network_logs)
    return;
  if (cfg.silent_keepalive && is_keepalive_packet(len))
    return;

  ensure_netlog_open();

  char dir = in ? '<' : '>';

  if (netlog_file) {
    fmt::print(netlog_file, "{} {} = ", dir, socket);
    std::fflush(netlog_file);

    for (size_t i = 0; i < len; ++i) {
      fmt::print(netlog_file, "{:02x}", static_cast<unsigned char>(data[i]));
    }
    fmt::println(netlog_file, "");
    std::fflush(netlog_file);
  }

  if (!cfg.silent_network) {
    logf(
        LogMessage(LogSource::Net, fmt::format("[net] {} socket {} len={}",
                                               in ? "rx" : "tx", socket, len)));
  }
}

void ensure_proudlog_open() {
  if (cfg.no_proud_logs || proudlog_file)
    return;

  proudlog_file = fopen(proud_log_path().c_str(), "a");
  if (proudlog_file) {
    fmt::println(proudlog_file, "# proudnet-tcp headers (tid port dir "
                                "chunk frames)");
    std::fflush(proudlog_file);
  }
}

void logpns(int socket, const char *addr, unsigned short port) {
  if (cfg.no_proud_logs)
    return;

  ensure_proudlog_open();
  if (!proudlog_file)
    return;

  const DWORD tid = GetCurrentThreadId();
  fmt::println(proudlog_file, "tid={} event=connect port={} sock={} addr={}",
               tid, port, socket, addr ? addr : "?");
  std::fflush(proudlog_file);

  if (!cfg.silent_proud)
    logf(LogMessage(LogSource::Proud,
                    fmt::format("connect socket {} port={} addr={}", socket,
                                port, addr ? addr : "?")));

  if (cfg.pipes) {
    Diagnostics::emit_proudnet_tcp_connect(
        tid, static_cast<unsigned long long>(socket), addr, port);
  }
}

void logpnf(int socket, const LogMessage &message) {
  if (cfg.no_proud_logs)
    return;

  ensure_proudlog_open();
  if (!proudlog_file)
    return;

  fmt::println(proudlog_file, "Socket {} : {}", socket, message.message);
  std::fflush(proudlog_file);

  if (!cfg.silent_proud)
    logf(message);
}

void logpnf(int socket, const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = fmt::format(format, args);
  va_end(args);
  logpnf(socket, LogMessage(LogSource::Proud, message));
}

void logpln(const LogMessage &message) {
  if (cfg.no_proud_logs)
    return;

  ensure_proudlog_open();
  if (!proudlog_file)
    return;

  fmt::println(proudlog_file, message.message);
  std::fflush(proudlog_file);

  if (!cfg.silent_proud)
    logf(message);
}

void logpln(const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = fmt::format(format, args);
  va_end(args);
  logpln(LogMessage(message));
}

void logpln_silent(const LogMessage &message) {
  if (cfg.no_proud_logs)
    return;

  ensure_proudlog_open();
  if (!proudlog_file)
    return;

  fmt::println(proudlog_file, message.message);
  std::fflush(proudlog_file);
}

void logpln_silent(const char *format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = fmt::format(format, args);
  va_end(args);
  logpln_silent(LogMessage(message));
}

void close_logs() {
  if (log_file) {
    fclose(log_file);
    log_file = nullptr;
  }
  if (netlog_file) {
    fclose(netlog_file);
    netlog_file = nullptr;
  }
  if (proudlog_file) {
    fclose(proudlog_file);
    proudlog_file = nullptr;
  }
}

} // namespace thegame
