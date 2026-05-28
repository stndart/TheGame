#include "game/net/socket_trace.hpp"

#include <unordered_map>
#include <unordered_set>
#include <windows.h>

namespace {

std::unordered_map<u_short, std::unordered_set<SOCKET>> g_sockets_by_port;
void *g_last_fast_socket_ctx = nullptr;

} // namespace

namespace SocketTrace {

bool is_pn_track_port(u_short port) {
  return port == kEntryPort || port == kGameLegPort || port == kProbePort;
}

bool is_tracked_pn_socket(SOCKET s) {
  if (s == INVALID_SOCKET)
    return false;
  const u_short peer = peer_port(s);
  if (is_pn_track_port(peer))
    return true;
  for (const auto &entry : g_sockets_by_port) {
    if (entry.second.count(s) != 0)
      return true;
  }
  return false;
}

u_short pn_log_port(SOCKET s) {
  const u_short peer = peer_port(s);
  if (peer != 0)
    return peer;
  for (const auto &entry : g_sockets_by_port) {
    if (entry.second.count(s) != 0)
      return entry.first;
  }
  return 0;
}

void track_connect(SOCKET s, u_short port) {
  if (s != INVALID_SOCKET && is_pn_track_port(port))
    g_sockets_by_port[port].insert(s);
}

u_short peer_port(SOCKET s) {
  if (s == INVALID_SOCKET)
    return 0;
  sockaddr_in peer{};
  int peer_len = sizeof(peer);
  if (getpeername(s, reinterpret_cast<sockaddr *>(&peer), &peer_len) != 0)
    return 0;
  if (peer.sin_family != AF_INET)
    return 0;
  return ntohs(peer.sin_port);
}

int net_log_key(SOCKET s) {
  const u_short port = peer_port(s);
  if (port != 0)
    return static_cast<int>(port);
  return static_cast<int>(s);
}

bool is_entry_socket(SOCKET s) {
  if (s == INVALID_SOCKET)
    return false;
  const auto it = g_sockets_by_port.find(kEntryPort);
  if (it != g_sockets_by_port.end() && it->second.count(s) != 0)
    return true;
  return peer_port(s) == kEntryPort;
}

bool is_game_server_socket(SOCKET s) { return is_entry_socket(s); }

bool peer_is_entry_server(SOCKET s) { return peer_port(s) == kEntryPort; }

static bool slot_memory_readable(SOCKET *slot) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(slot, &mbi, sizeof(mbi)) == 0)
    return false;
  return (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY |
                         PAGE_EXECUTE_WRITECOPY)) != 0;
}

static bool can_write_socket_slot(void *ctx, SOCKET *slot) {
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(ctx, &mbi, sizeof(mbi)) == 0 ||
      (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY |
                      PAGE_EXECUTE_WRITECOPY)) == 0)
    return false;
  if (VirtualQuery(slot, &mbi, sizeof(mbi)) == 0 ||
      (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY |
                      PAGE_EXECUTE_WRITECOPY)) == 0)
    return false;
  return true;
}

SOCKET read_fast_socket_slot(void *fast_socket_ctx) {
  if (!fast_socket_ctx)
    return INVALID_SOCKET;
  auto *sock_slot =
      reinterpret_cast<SOCKET *>(static_cast<char *>(fast_socket_ctx) + 300);
  if (!slot_memory_readable(sock_slot))
    return INVALID_SOCKET;
  return *sock_slot;
}

static SOCKET pick_tracked_entry_socket() {
  const auto it = g_sockets_by_port.find(kEntryPort);
  if (it == g_sockets_by_port.end())
    return INVALID_SOCKET;
  for (SOCKET tracked : it->second) {
    if (peer_is_entry_server(tracked))
      return tracked;
  }
  return INVALID_SOCKET;
}

void note_fast_socket_ctx(void *fast_socket_ctx) {
  g_last_fast_socket_ctx = fast_socket_ctx;
}

void *last_fast_socket_ctx() { return g_last_fast_socket_ctx; }

SOCKET socket_from_last_fast_ctx() {
  return read_fast_socket_slot(g_last_fast_socket_ctx);
}

u_short fast_socket_log_port(void *fast_socket_ctx) {
  if (!fast_socket_ctx)
    return 0;

  const SOCKET sock = read_fast_socket_slot(fast_socket_ctx);
  const u_short peer = peer_port(sock);
  if (peer != 0)
    return peer;

  struct AddrPort {
    unsigned char addr[16];
    u_short port;
  };
  const auto *ap = reinterpret_cast<const AddrPort *>(
      static_cast<const char *>(fast_socket_ctx) + 0xE4);
  return ap->port;
}

void sync_fast_socket_handle(void *fast_socket_ctx) {
  if (!fast_socket_ctx)
    return;
  auto *sock_slot =
      reinterpret_cast<SOCKET *>(static_cast<char *>(fast_socket_ctx) + 300);
  if (!can_write_socket_slot(fast_socket_ctx, sock_slot))
    return;

  const SOCKET slot_sock = *sock_slot;
  const u_short slot_peer = peer_port(slot_sock);

  // GAME / PROBE legs must keep their own fd - overwriting caused 0x2f on
  // :7000.
  if (slot_peer == kGameLegPort || slot_peer == kProbePort)
    return;
  if (slot_peer == kEntryPort)
    return;

  const SOCKET entry = pick_tracked_entry_socket();
  if (entry == INVALID_SOCKET)
    return;
  if (slot_sock == entry)
    return;

  *sock_slot = entry;
}

} // namespace SocketTrace
