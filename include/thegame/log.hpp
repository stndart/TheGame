#pragma once

#include <cstddef>

#include <fmt/color.h>
#include <map>

struct _EXCEPTION_POINTERS;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;

template <typename... Args>
using format_string =
    fmt::basic_format_string<char, fmt::type_identity_t<Args>...>;

namespace thegame {

enum LogSource { Exception, Log, Net, Proud, RMI, Stage, Nav };

enum LogImportance { Default, Seen, NotSeen, Milestone, Warning };

fmt::rgb color_for_importance(LogImportance importance);

std::string source_to_prefix(LogSource source);

extern std::map<LogSource, bool> silenced;
extern std::map<LogSource, bool> file_silenced;

class LogMessage {
public:
  std::string message;
  LogSource source = Log;
  LogImportance kind = Default;

  LogMessage(const char *message) : message(message ? message : "") {}

  LogMessage(std::string message) : message(message) {}

  template <typename... Args>
  LogMessage(format_string<Args...> format, Args &&...args) {
    message = fmt::format(format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  LogMessage(LogSource source, format_string<Args...> format, Args &&...args)
      : source(source) {
    message = fmt::format(format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  LogMessage(LogSource source, LogImportance kind,
             format_string<Args...> format, Args &&...args)
      : source(source), kind(kind) {
    message = fmt::format(format, std::forward<Args>(args)...);
  }

  LogMessage with_kind(LogImportance kind) const {
    return LogMessage(source, kind, message);
  }

  LogMessage with_source(LogSource source) const {
    return LogMessage(source, kind, message);
  }

public:
  // writes to file, optionally with prefix
  void write_to(FILE *file, bool prefix = true) const;
  // writes to console, always with prefix
  void write_to_console() const;
};

extern bool console_created;

void create_console();
void log_boot_paths();

void prepare_logs();
void close_logs();

// general console log + logs.txt
void logf(const LogMessage &message);

// netlogs.txt (+ optional [net] console line);
// gated by NO_NETWORK_LOGS / SILENT_NETWORK.
void logn(int socket, const LogMessage &message);
void logn(int socket, const char *addr, int port);
void logn(int socket, size_t len, char *data, bool in = false);

// proudlogs.txt (+ optional [proud] console line);
// gated by NO_PROUD_LOGS / SILENT_PROUD.
void logp(int socket, const LogMessage &message);
void logp(const LogMessage &message);
void logp_silent(const LogMessage &message); // doesn't print to console

void exceptionf(EXCEPTION_POINTERS *info, const char *type = "exception");
void exceptionf(const LogMessage &message);

void eventf(const LogMessage &message);
void stagef(const LogMessage &message);

bool is_keepalive_packet(size_t len);

} // namespace thegame
