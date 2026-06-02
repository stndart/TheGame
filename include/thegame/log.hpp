#pragma once

#include <cstddef>
#include <fstream>

struct _EXCEPTION_POINTERS;
typedef struct _EXCEPTION_POINTERS EXCEPTION_POINTERS;

namespace thegame {

extern std::ofstream log_file;
extern std::ofstream netlog_file;
extern bool console_created;

void create_console();
void log_message(const char *message);
void logf(const char *format, ...);

// netlogs.txt (+ optional [net] console line); gated by NO_NETWORK_LOGS /
// SILENT_NETWORK.
void logns(int socket, const char *addr, int port);
void lognf(int socket, const char *format, ...);
void logn(int socket, size_t len, char *data, bool in = false);

// proudlogs.txt (+ optional [proud] console line); gated by NO_PROUD_LOGS /
// SILENT_PROUD.
void logpns(int socket, const char *addr, unsigned short port);
void logpnf(int socket, const char *format, ...);
void logpln(const char *line);

void exceptionf(const char *type, EXCEPTION_POINTERS *info,
                const char *format = "", ...);
void eventf(const char *format, ...);
void stagef(const char *stage);

void log_boot_paths();

void close_logs();

bool is_keepalive_packet(size_t len);

} // namespace thegame
