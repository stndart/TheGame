#include "thegame/log.hpp"

#include <cstdio>

#include <fmt/base.h>
#include <fmt/ranges.h>

#include "diagnostics/handlers.hpp"
#include "thegame/config.hpp"
#include "thegame/paths.hpp"

namespace thegame {

fmt::rgb color_for_importance(LogImportance kind) {
  switch (kind) {
  case Seen:
    return fmt::rgb(64, 192, 64);
  case NotSeen:
    return fmt::rgb(192, 64, 64);
  case Milestone:
    return fmt::rgb(128, 192, 192);
  case Warning:
    return fmt::rgb(255, 192, 64);
  case Default:
  default:
    return fmt::color::white;
  }
}

std::string source_to_prefix(LogSource source) {
  switch (source) {
  case Exception:
    return "[!exception] ";
  case Log:
    return "[log] ";
  case Net:
    return "[net] ";
  case Proud:
    return "[proud] ";
  case RMI:
    return "[rmi] ";
  case Stage:
    return "[stage] ";
  case Nav:
    return "[nav] ";
  default:
    return "";
  }
}

std::map<LogSource, bool> silenced;
std::map<LogSource, bool> file_silenced;

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

void log_boot_paths() {
  logf(LogMessage(fmt::format("thegame logs: main={} net={} proud={}",
                              main_log_path(), net_log_path(),
                              proud_log_path())));
  logf(LogMessage(fmt::format(
      "thegame flags: hooks={} entrypoint={} veh={} pipes={}",
      cfg.disable_hooks ? 0 : 1, cfg.disable_entrypoint_hook ? 0 : 1,
      cfg.disable_veh ? 0 : 1, cfg.pipes ? 1 : 0)));

  logf(LogMessage(
      fmt::format("thegame log flags: no_net={} silent_net={} no_proud={} "
                  "silent_proud={} "
                  "silent_keepalive={}",
                  cfg.no_network_logs ? 1 : 0, cfg.silent_network ? 1 : 0,
                  cfg.no_proud_logs ? 1 : 0, cfg.silent_proud ? 1 : 0,
                  cfg.silent_keepalive ? 1 : 0)));
}

void prepare_logs() {
  if (!log_file)
    log_file = fopen(main_log_path().c_str(), "a");
  if (!netlog_file)
    netlog_file = fopen(net_log_path().c_str(), "a");
  if (!proudlog_file)
    proudlog_file = fopen(proud_log_path().c_str(), "a");

  // make sure you have all the keys here. segfault otherwise.
  for (LogSource source : {Exception, Log, Net, Proud, RMI, Stage, Nav}) {
    silenced[source] = false;
    file_silenced[source] = false;
  }

  if (cfg.no_network_logs) {
    silenced[Net] = true;
    file_silenced[Net] = true;
  }
  if (cfg.no_proud_logs) {
    silenced[Proud] = true;
    file_silenced[Proud] = true;
  }

  if (cfg.silent_network)
    silenced[Net] = true;
  if (cfg.silent_proud)
    silenced[Proud] = true;
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

void LogMessage::write_to(FILE *file, bool prefix) const {
  if (!file)
    return;

  // protect fmt print from {} injections
  if (prefix)
    fmt::print(file, "{}", source_to_prefix(source));
  fmt::println(file, "{}", message);
  std::fflush(file);
}

void LogMessage::write_to_console() const {
  if (!cfg.no_console) {
    if (!console_created)
      create_console();

    // protect fmt print from {} injections
    if (!cfg.no_colors) {
      fmt::text_style fg = fmt::fg(color_for_importance(kind));
      fmt::print(fg, "{}", source_to_prefix(source));
      fmt::print(fg, "{}", message);
      fmt::println("");
    } else {
      fmt::print("{}", source_to_prefix(source));
      fmt::println("{}", message);
    }
  }
}

void logf(const LogMessage &message) {
  if (!silenced[message.source])
    message.write_to_console();

  if (!file_silenced[message.source])
    message.write_to(log_file);

  // Exception and Stage have their own emits
  if (cfg.pipes && message.source != Exception && message.source != Stage)
    Diagnostics::emit_game_log(message.message.c_str());
}

void logn(int socket, const LogMessage &message) {
  LogMessage line(Net, "sock {}: {}", socket, message.message);
  logf(line);
  line.write_to(netlog_file);
}

void logn(int socket, const char *addr, int port) {
  LogMessage line(Net, "connect socket {} to {}:{}", socket, addr, port);
  logf(line);
  line.write_to(netlog_file);
}

void logn(int socket, size_t len, char *data, bool in) {
  if (cfg.no_network_logs)
    return;

  // Keepalives are filtered by port (27000) or ProudNet RMI opcode (0x1C).
  // if (cfg.silent_keepalive && is_keepalive_packet(len))
  //   return;

  LogMessage line(Net, "{} sock {} len={}", in ? "rx" : "tx", socket, len);
  logf(line);

  const auto *bytes = reinterpret_cast<const unsigned char *>(data);
  const auto hex = fmt::format("{:02x}", fmt::join(bytes, bytes + len, " "));

  LogMessage data_line(Net, "{} sock {} data[{}]: {}", in ? "rx" : "tx", socket,
                       len, hex);
  data_line.write_to(netlog_file);
}

void logp(int socket, const LogMessage &message) {
  LogMessage line(Proud, "sock {}: {}", socket, message.message);
  logf(line);
  line.write_to(proudlog_file);
}

void logp(const LogMessage &message) {
  const LogMessage line = message.with_source(Proud);
  logf(line);
  line.write_to(proudlog_file);
}

void logp_silent(const LogMessage &message) { message.write_to(proudlog_file); }

void exceptionf(EXCEPTION_POINTERS *info, const char *type) {
  // VEH-safe: no fmt/LogMessage (C++ in first-chance handlers breaks unwind).
  char buffer[512];
  _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "[!exception] %s: 0x%08lX",
              type ? type : "exception", info->ExceptionRecord->ExceptionCode);
  if (log_file) {
    std::fputs(buffer, log_file);
    std::fputc('\n', log_file);
    std::fflush(log_file);
  }
  if (!cfg.no_console) {
    if (!console_created)
      create_console();
    std::fputs(buffer, stdout);
    std::fputc('\n', stdout);
  }
  Diagnostics::emit_exception_event(type, info);
}

void exceptionf(const LogMessage &message) {
  logf(message.with_source(Exception));
  Diagnostics::emit_custom_exception(message.message.c_str());
}

// the same as logf, but doesn't print to console
void eventf(const LogMessage &message) {
  if (cfg.pipes)
    Diagnostics::emit_game_log(message.message.c_str());
}

void stagef(const LogMessage &message) {
  logf(message.with_source(Stage).with_kind(Milestone));
  if (cfg.pipes)
    Diagnostics::emit_game_stage(message.message.c_str());
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

bool is_keepalive_packet(size_t len) { return len <= 8; }

} // namespace thegame
