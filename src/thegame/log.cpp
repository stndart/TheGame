#include "thegame/log.hpp"

#include "diagnostics/handlers.hpp"
#include "thegame/config.hpp"
#include "thegame/paths.hpp"

#include <cstring>

#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <windows.h>

#include <fmt/base.h>

namespace thegame {

fmt::rgb color_from_kind(ColorKind kind) {
  switch (kind) {
  case White:
    return fmt::color::white;
  case Seen:
    return fmt::rgb(64, 192, 64);
  case Unknown:
    return fmt::rgb(192, 64, 64);
  case Interesting:
    return fmt::rgb(192, 192, 64);
    case
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
  console_created = true;
}

void log_message(const LogMessage &message) {
  if (!cfg.no_console) {
    if (!console_created)
      create_console();
    fmt::print(fmt::fg(color_from_kind(message.kind)), message.message);
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

void exceptionf(const char *type, EXCEPTION_POINTERS *info,
                const LogMessage &message) {
  log_message(message);
  if (info)
    Diagnostics::emit_exception_event(type, info, message.message.c_str());
  else
    Diagnostics::emit_custom_exception(message.message.c_str());
}

// the same as logf, but doesn't print to console
void eventf(const LogMessage &message) {
  if (cfg.pipes)
    Diagnostics::emit_game_log(message.message.c_str());
}

void stagef(const LogMessage &message) {
  log_message(message.with_kind(Stage));
  if (cfg.pipes)
    Diagnostics::emit_game_stage(message.message.c_str());
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
        logf(LogMessage(LogSource::Net,
                        fmt::format("[net] connect socket {} to {}:{}", socket,
                                    addr, port)));
    }

    void lognf(int socket, const char *format, ...) {
      if (cfg.no_network_logs)
        return;

      ensure_netlog_open();

      if (netlog_file) {
        fmt::println(netlog_file, "Error with socket {} : {}", socket, format);
        std::fflush(netlog_file);
      }

      if (!cfg.silent_network)
        logf(
            LogMessage(LogSource::Net, fmt::format("[net] error socket {} : {}",
                                                   socket, format)));
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
        proudlog_file << "# proudnet-tcp headers (tid port dir "
                         "chunk frames)\n";
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
