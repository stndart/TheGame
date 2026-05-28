#pragma once

#include <cstddef>
#include <winsock2.h>

namespace PnTcpTrace {

constexpr const char *kLogFileName = "proudnet_tcp.txt";

void log_connect(SOCKET sock, const char *addr, u_short port);
void log_chunk(SOCKET sock, const void *data, size_t len, bool inbound);

void close_log_file();

} // namespace PnTcpTrace
