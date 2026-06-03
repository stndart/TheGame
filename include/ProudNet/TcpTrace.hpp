#pragma once

#include <cstddef>
#include <winsock2.h>

namespace Proud {
namespace TcpTrace {

constexpr const char *kLogFileName = "proudlogs.txt";

void log_connect(SOCKET sock, const char *addr, u_short port);
void log_chunk(SOCKET sock, const void *data, size_t len, bool inbound,
               void *fast_socket_ctx = nullptr);

} // namespace TcpTrace
} // namespace Proud
