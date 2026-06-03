#pragma once

#include <cstdarg>
#include <cstddef>

#include <fmt/color.h>

struct _EXCEPTION_POINTERS;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;

namespace thegame {

enum LogSource { Exception, Log, Net, Proud, RMI, Stage, Nav };

enum ColorKind {
  White,
  Seen,
  Unknown,
  Interesting,
  Milestone,
};

fmt::rgb color_from_kind(ColorKind kind);

std::string source_to_prefix(LogSource source);

struct LogMessage {
  LogSource source;
  std::string message;
  ColorKind kind = White;

  LogMessage(std::string message) {
    this->message = message;
    source = Log;
  }

  LogMessage(LogSource source, std::string message) {
    this->message = message;
    this->source = source;
  }

  LogMessage(LogSource source, ColorKind kind, std::string message) {
    this->message = message;
    this->source = source;
    this->kind = kind;
  }

  LogMessage(LogSource source, const char *format, ...) {
    va_list args;
    va_start(args, format);
    message = fmt::format(format, args);
    va_end(args);
    this->source = source;
  }

  LogMessage(LogSource source, ColorKind kind, const char *format, ...) {
    va_list args;
    va_start(args, format);
    message = fmt::format(format, args);
    va_end(args);
    this->source = source;
    this->kind = kind;
  }

  LogMessage with_kind(ColorKind kind) const {
    return LogMessage(source, kind, message.c_str());
  }
};

extern bool console_created;

void create_console();
void log_message(const LogMessage &message);
void logf(const LogMessage &message);
void logf(const char *format, ...);

// netlogs.txt (+ optional [net] console line);
// gated by NO_NETWORK_LOGS / SILENT_NETWORK.
void logns(int socket, const char *addr, int port);
void lognf(int socket, const LogMessage &message);
void lognf(int socket, const char *format, ...);
void logn(int socket, size_t len, char *data, bool in = false);

// proudlogs.txt (+ optional [proud] console line);
// gated by NO_PROUD_LOGS / SILENT_PROUD.
void logpns(int socket, const char *addr, unsigned short port);
void logpnf(int socket, const LogMessage &message);
void logpnf(int socket, const char *format, ...);
void logpln(const LogMessage &message);
void logpln(const char *format, ...);
void logpln_silent(const LogMessage &message); // doesn't print to console
void logpln_silent(const char *format, ...);

void exceptionf(const char *type, EXCEPTION_POINTERS *info,
                const LogMessage &message);
void exceptionf(const char *type, EXCEPTION_POINTERS *info,
                const char *format = "", ...);
void eventf(const LogMessage &message);
void stagef(const char *stage);

void log_boot_paths();

void close_logs();

bool is_keepalive_packet(size_t len);

} // namespace thegame
