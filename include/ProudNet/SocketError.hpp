#pragma once

namespace Proud {

// sub_D56170 - shared by TCPSocket::Connect @ 0xD56220 and CFastSocket paths.
void SocketReportError(void *socket_obj, int err, void *msg_ctx = nullptr);

} // namespace Proud
