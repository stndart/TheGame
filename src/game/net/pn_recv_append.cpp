#include "game/net/pn_recv_append.hpp"

#include "game/net/pn_fast_socket.hpp"
#include "game/net/pn_growable.hpp"
#include "game/net/pn_layout.hpp"
#include "game/net/pn_tcp_trace.hpp"
#include "game/net/socket_trace.hpp"

namespace {

void *fast_socket_from_recv_buffer(const PNRecvBuffer *recv) {
  if (!recv)
    return nullptr;
  auto *conn = reinterpret_cast<char *>(const_cast<PNRecvBuffer *>(recv)) -
               pn::conn::kRecvBuffer;
  auto **fast_pp =
      reinterpret_cast<PNFastSocket **>(conn + pn::conn::kFastSocket);
  if (!fast_pp || !*fast_pp)
    return nullptr;
  return *fast_pp;
}

void log_recv_chunk(const PNRecvBuffer *recv, const char *src, int len) {
  if (!src || len <= 0)
    return;

  void *const fast = fast_socket_from_recv_buffer(recv);
  if (fast)
    SocketTrace::note_fast_socket_ctx(fast);

  SOCKET sock = INVALID_SOCKET;
  if (fast)
    sock = SocketTrace::read_fast_socket_slot(fast);
  if (sock == INVALID_SOCKET)
    sock = SocketTrace::socket_from_last_fast_ctx();

  PnTcpTrace::log_chunk(sock, src, static_cast<size_t>(len), true, fast);
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

  log_recv_chunk(this, src, len);
}
