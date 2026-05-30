#include "ProudNet/SelectFd.hpp"

#include <winsock2.h>

namespace Proud {
namespace select_fd {

namespace {

unsigned add_socket_to_set(unsigned *counts_base, unsigned *fds_base,
                         SOCKET sock) {
  const unsigned count = *counts_base;
  unsigned i = 0;
  if (count) {
    for (; i < count; ++i) {
      if (fds_base[i] == static_cast<unsigned>(sock))
        break;
    }
  }
  if (i == count && count < FD_SETSIZE) {
    fds_base[count] = static_cast<unsigned>(sock);
    ++(*counts_base);
  }
  return i;
}

} // namespace

void init(Proud::CSelectContext *ctx) {
  auto *d = reinterpret_cast<unsigned *>(ctx);
  d[0] = 0;
  d[65] = 0;
  d[130] = 0;
}

void add_read(Proud::CSelectContext *ctx, Proud::CFastSocket *fast) {
  if (!fast)
    return;
  auto *base = reinterpret_cast<char *>(fast);
  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + FastSocketLayout::kSocket);
  auto *d = reinterpret_cast<unsigned *>(ctx);
  add_socket_to_set(&d[65], &d[66], sock);
}

void add_except(Proud::CSelectContext *ctx, Proud::CFastSocket *fast) {
  if (!fast)
    return;
  auto *base = reinterpret_cast<char *>(fast);
  const SOCKET sock =
      *reinterpret_cast<SOCKET *>(base + FastSocketLayout::kSocket);
  auto *d = reinterpret_cast<unsigned *>(ctx);
  add_socket_to_set(&d[130], &d[131], sock);
}

int wait(Proud::CSelectContext *ctx, unsigned timeout_ms) {
  auto *readfds = reinterpret_cast<fd_set *>(ctx);
  timeval tv{};
  timeval *ptv = nullptr;
  if (timeout_ms != static_cast<unsigned>(-1)) {
    tv.tv_sec = static_cast<long>(timeout_ms / 1000);
    tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
    ptv = &tv;
  }
  return select(0, readfds, readfds + 1, readfds + 2, ptv);
}

} // namespace select_fd
} // namespace Proud
