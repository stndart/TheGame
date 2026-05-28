#pragma once

namespace pn {

// sub_D56170 - shared by TCPSocket::Connect @ 0xD56220 and PNFastSocket paths.
// Uses pn::fast_socket::* offsets; `socket_obj` is the CFastSocket / TCPSocket
// `this`.
void socket_report_error(void *socket_obj, int err, void *msg_ctx = nullptr);

} // namespace pn
