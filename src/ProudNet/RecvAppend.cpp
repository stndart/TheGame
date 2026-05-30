#include "ProudNet/RecvAppend.hpp"

#include "ProudNet/FastSocket.hpp"
#include "ProudNet/GrowableBuffer.hpp"
#include "ProudNet/Layout.hpp"
#include "ProudNet/TcpTrace.hpp"
#include "game/net/socket_trace.hpp"

namespace {

void *fast_socket_from_recv_buffer(const Proud::RecvBuffer *recv) {
  if (!recv)
    return nullptr;
  auto *conn = reinterpret_cast<char *>(const_cast<Proud::RecvBuffer *>(recv)) -
               Proud::conn::kRecvBuffer;
  auto **fast_pp =
      reinterpret_cast<Proud::CFastSocket **>(conn + Proud::conn::kFastSocket);
  if (!fast_pp || !*fast_pp)
    return nullptr;
  return *fast_pp;
}

void log_recv_chunk(const Proud::RecvBuffer *recv, const char *src, int len) {
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

  Proud::TcpTrace::log_chunk(sock, src, static_cast<size_t>(len), true, fast);
}

} // namespace

void Proud::RecvBuffer::ensure_capacity(int new_size) {
  Proud::growable::ensure_capacity_copy(*this, new_size);
}

void Proud::RecvBuffer::append(const char *src, int len) {
  if (len < 0)
    Proud::growable::throw_negative_size();
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
