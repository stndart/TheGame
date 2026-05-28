#include "game/net/pn_recv_append.hpp"

#include "game/net/pn_growable.hpp"
#include "game/net/pn_tcp_trace.hpp"
#include "game/net/socket_trace.hpp"

namespace {

void log_recv_chunk(const char *src, int len) {
  if (!src || len <= 0)
    return;
  if (!SocketTrace::last_fast_socket_ctx())
    return;
  const SOCKET sock = SocketTrace::socket_from_last_fast_ctx();
  PnTcpTrace::log_chunk(sock, src, static_cast<size_t>(len), true);
}

} // namespace

void PNRecvBuffer::ensure_capacity(int new_size) {
  pn::growable::ensure_capacity_copy(*this, new_size);
}

void PNRecvBuffer::append(const char *src, int len) {
  if (len < 0)
    pn::growable::throw_negative_size();
  if (len == 0)
    return;

  const int cur_size = size_;
  ensure_capacity(cur_size + len);

  char *dest = nullptr;
  if (size_ != 0)
    dest = data_;

  for (int i = 0; i < len; ++i)
    dest[cur_size + i] = src[i];

  log_recv_chunk(src, len);
}
