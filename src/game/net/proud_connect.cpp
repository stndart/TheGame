#include "game/net/proud_connect.hpp"

#include <cstdio>
#include <cstring>
#include <ws2tcpip.h>

#include "game/server_override.hpp"
#include "system_hooks.h"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

using thegame::logf;
using thegame::lognf;
using thegame::logns;

namespace {

constexpr char kIfPrefix[] = "if!";
constexpr char kHostPrefix[] = "host!";

// w_connect_2 stack: int[4] sockopts then sockaddr_in (open_socket copies opts
// @ +16).
struct ConnectSockbuf {
  int fields[4];
  sockaddr_in name;
};

// sub_1431030 - copy peer fields from ctx into mgr (+34524, +34572, ...).
static void copy_ctx_fields(int *ctx) {
  auto *const mgr = reinterpret_cast<unsigned char *>(*ctx);
  auto *const ctx_bytes = reinterpret_cast<unsigned char *>(ctx);
  memcpy(mgr + 34524, ctx_bytes + 158, 0x2Eu);
  memcpy(mgr + 34576, ctx + 52, 0x2Eu);
  *reinterpret_cast<int *>(mgr + 34572) = ctx[51];
  *reinterpret_cast<int *>(mgr + 34624) = ctx[64];
}

// sub_1433F70 - format error into ctx+684 (255 chars), return pointer.
static const char *format_wsa_error(int *ctx, unsigned long code) {
  char *const out = reinterpret_cast<char *>(ctx) + 684;
  out[0] = '\0';

  const DWORD saved_err = GetLastError();

  if (code < static_cast<unsigned long>(_sys_nerr)) {
    const char *s = strerror(static_cast<int>(code));
    if (s)
      strncpy(out, s, 255);
  } else if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, out, 255, nullptr)) {
    snprintf(out, 255, "Unknown error %lu (%#lx)", code, code);
  }
  out[255] = '\0';

  char *nl = strrchr(out, '\n');
  if (nl && nl - out >= 2)
    *nl = '\0';
  nl = strrchr(out, '\r');
  if (nl && nl - out >= 1)
    *nl = '\0';

  if (GetLastError() != saved_err)
    SetLastError(saved_err);
  return out;
}

// sub_1431230 - TCP_NODELAY from mgr+788.
static void tcp_nodelay(SOCKET s, int *ctx, unsigned *mgr) {
  int on = static_cast<int>(mgr[197]);
  if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char *>(&on), sizeof(on)) >= 0) {
    lognf(static_cast<int>(s), "TCP_NODELAY set");
    return;
  }
  const int err = WSAGetLastError();
  if (!thegame::cfg.no_network_logs)
    lognf(static_cast<int>(s), "Could not set TCP_NODELAY: %s",
          format_wsa_error(ctx, static_cast<unsigned long>(err)));
}

// sub_1431290 - raise SO_RCVBUF to at least 16416 when current is smaller.
static void recvbuf_floor(SOCKET s) {
  constexpr int kMinRecv = 16416;
  int cur = 0;
  int optlen = sizeof(cur);
  if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char *>(&cur),
                 &optlen) != 0 ||
      cur <= kMinRecv) {
    const int val = kMinRecv;
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&val),
               sizeof(val));
  }
}

// sub_1430AC0 - SO_KEEPALIVE from mgr+1032.
static void set_keepalive(unsigned *mgr, SOCKET s) {
  int on = mgr[258] != 0;
  if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE,
                 reinterpret_cast<const char *>(&on), sizeof(on)) < 0)
    if (!thegame::cfg.no_network_logs)
      lognf(static_cast<int>(s), "Failed to set SO_KEEPALIVE");
}

// sub_144D2A0 - non-blocking mode.
static void set_nonblock(SOCKET s, u_long on) {
  u_long mode = on ? 1u : 0u;
  ioctlsocket(s, FIONBIO, &mode);
}

// sub_14313B0 - close socket (optional ctx close hook).
static int close_socket_ctx(int *ctx, SOCKET s) {
  if (!ctx)
    return closesocket(s);
  if (!ctx[10])
    return closesocket(s);
  if (s == static_cast<SOCKET>(ctx[77]) && ctx[79]) {
    ctx[79] = 0;
    return closesocket(s);
  }
  using CloseCb = int(__cdecl *)(int);
  return reinterpret_cast<CloseCb>(ctx[10])(ctx[11]);
}

// sub_142D0D0 - libcurl timer splay; not needed for offline connect timeout.
static void timer_splay_connect(void * /*mgr*/, int /*ms*/) {}

// sub_1431400 - build socket() args from param4, optional factory @ mgr+372.
static int open_socket(int *ctx, int param4, char *sockopts, SOCKET *out_sock) {
  char local_opts[144];
  char *const opts = sockopts ? sockopts : local_opts;

  auto *const mgr = reinterpret_cast<unsigned *>(*ctx);
  const int af = *reinterpret_cast<int *>(param4 + 4);
  const int type = ctx[29];
  int protocol = 17;
  if (type == SOCK_DGRAM)
    protocol = *reinterpret_cast<int *>(param4 + 12);

  unsigned opt_len = *reinterpret_cast<unsigned *>(param4 + 16);
  if (opt_len > 0x80u)
    opt_len = 0x80u;

  reinterpret_cast<int *>(opts)[0] = af;
  reinterpret_cast<int *>(opts)[1] = type;
  reinterpret_cast<int *>(opts)[2] = protocol;
  reinterpret_cast<unsigned *>(opts)[3] = opt_len;
  if (opt_len)
    memcpy(opts + 16, *reinterpret_cast<const void **>(param4 + 24), opt_len);

  using SocketFactoryFn = SOCKET(__cdecl *)(unsigned, unsigned, char *);
  const auto factory = reinterpret_cast<SocketFactoryFn>(mgr[93]); // mgr+372
  SOCKET s;
  if (factory)
    s = factory(mgr[94], 0, opts); // mgr+376
  else
    s = socket(af, type, protocol);

  *out_sock = s;
  return s == INVALID_SOCKET ? 7 : 0;
}

// sub_1434110 - dotted IPv4 → sin_addr (returns positive length on success).
static int parse_ipv4_dotted(const char *s, in_addr *out) {
  if (!s || !*s)
    return 0;
  if (InetPtonA(AF_INET, s, out) == 1)
    return static_cast<int>(strlen(s)) + 1;

  unsigned char octets[4]{};
  int count = 0;
  const char *p = s;
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      unsigned v = 0;
      while (*p >= '0' && *p <= '9') {
        v = v * 10u + static_cast<unsigned>(*p - '0');
        if (v > 255u)
          return 0;
        ++p;
      }
      if (count == 4)
        return 0;
      octets[count++] = static_cast<unsigned char>(v);
      if (!*p)
        break;
      if (*p != '.')
        return 0;
      ++p;
      continue;
    }
    return 0;
  }
  if (count != 4)
    return 0;
  memcpy(&out->S_un.S_addr, octets, 4);
  return static_cast<int>(p - s) + 1;
}

static bool format_ipv4(in_addr addr, char *buf, size_t buf_len) {
  unsigned char b[4];
  memcpy(b, &addr.S_un.S_addr, 4);
  const int n = snprintf(buf, buf_len, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
  return n > 0 && static_cast<size_t>(n) < buf_len;
}

// sub_14341D0 / sub_1431090 - format peer IP string into ctx+0x9E, port @
// ctx+0xCC.
static bool resolve_sockaddr(int *ctx, const sockaddr_in *peer) {
  if (peer->sin_family != AF_INET)
    return false;

  auto *const ctx_bytes = reinterpret_cast<unsigned char *>(ctx);
  char host[46]{};
  if (!format_ipv4(peer->sin_addr, host, sizeof(host)))
    return false;

  memcpy(ctx_bytes + 0x9E, host, 0x2Eu);
  *reinterpret_cast<unsigned short *>(ctx_bytes + 0xCC) = htons(peer->sin_port);
  return true;
}

static bool connect_is_pending(int err) {
  return err == WSAEWOULDBLOCK || (err > 10034 && err <= 10036);
}

static int connect_with_remap(SOCKET s, sockaddr *name, int namelen) {
  if (namelen >= static_cast<int>(sizeof(sockaddr_in)) &&
      name->sa_family == AF_INET) {
    auto *in = reinterpret_cast<sockaddr_in *>(name);
    const u_short port = ntohs(in->sin_port);
    in_addr was = in->sin_addr;
    if (ServerOverride::remap_sockaddr_in(in) &&
        port == ServerOverride::kGameLegPort)
      logf("w_connect_2:27380 %s -> %s", inet_ntoa(was),
           inet_ntoa(in->sin_addr));
  }
  return connect_syshandle(s, name, namelen);
}

static bool starts_with_prefix(const char *s, const char *prefix) {
  const size_t n = strlen(prefix);
  return strncmp(s, prefix, n) == 0;
}

static const char *strip_prefix(const char *s, const char *prefix) {
  return starts_with_prefix(s, prefix) ? s + strlen(prefix) : s;
}

// sub_144D290 - interface name → IP (stub returns 0 in this GAME build).
static bool resolve_local_interface(int af, const char *iface, char *out,
                                    size_t out_len) {
  (void)af;
  (void)iface;
  (void)out;
  (void)out_len;
  return false;
}

static bool resolve_hostname(int af, const char *host, char *out,
                             size_t out_len) {
  addrinfo hints{};
  hints.ai_family = (af == AF_INET) ? AF_INET : AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *res = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res)
    return false;

  bool ok = false;
  for (addrinfo *p = res; p && !ok; p = p->ai_next) {
    if (p->ai_family != AF_INET || !p->ai_addr)
      continue;
    const auto *in = reinterpret_cast<sockaddr_in *>(p->ai_addr);
    ok = format_ipv4(in->sin_addr, out, out_len);
  }
  freeaddrinfo(res);
  return ok;
}

// w_bind_3 @ 0x1430BF0
static int w_bind_3(int *ctx, SOCKET s, int af) {
  auto *const mgr = reinterpret_cast<unsigned *>(*ctx);

  u_short port = *reinterpret_cast<u_short *>(mgr + 80);           // +320
  int retries = *reinterpret_cast<int *>(mgr + 81);                // +324
  const char *iface = *reinterpret_cast<const char **>(mgr + 213); // +852

  if (!iface && !port)
    return 0;

  sockaddr_in name{};
  int namelen = 0;
  char ip_text[256]{};

  if (iface && strlen(iface) < 0xFFu) {
    const char *host = iface;
    if (starts_with_prefix(host, kIfPrefix)) {
      host = strip_prefix(host, kIfPrefix);
      if (!resolve_local_interface(af, host, ip_text, sizeof(ip_text)))
        return 45;
      logf("Local Interface %s is ip %s using address family %i", host, ip_text,
           af);
    } else {
      if (starts_with_prefix(host, kHostPrefix))
        host = strip_prefix(host, kHostPrefix);

      auto *const ctx_bytes = reinterpret_cast<unsigned char *>(ctx);
      const int saved = *reinterpret_cast<int *>(ctx_bytes + 540);
      if (af == AF_INET)
        *reinterpret_cast<int *>(ctx_bytes + 540) = 1;

      if (!resolve_hostname(af, host, ip_text, sizeof(ip_text))) {
        *reinterpret_cast<int *>(ctx_bytes + 540) = saved;
        logf("Couldn't bind to '%s'", iface);
        return 45;
      }

      *reinterpret_cast<int *>(ctx_bytes + 540) = saved;
      logf("Name '%s' family %i resolved to '%s' family %i", host, af, ip_text,
           af);
    }

    if (af == AF_INET && parse_ipv4_dotted(ip_text, &name.sin_addr) > 0) {
      name.sin_family = AF_INET;
      name.sin_port = htons(port);
      namelen = sizeof(name);
    }
  } else if (af == AF_INET) {
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    namelen = sizeof(name);
  }

  if (namelen == 0)
    return 0;

  if (bind(s, reinterpret_cast<sockaddr *>(&name), namelen) >= 0)
    goto bound_ok;

  while (--retries > 0) {
    logf("Bind to local port %hu failed, trying next", port);
    ++port;
    if (name.sin_family == AF_INET)
      name.sin_port = htons(port);
    if (bind(s, reinterpret_cast<sockaddr *>(&name), namelen) >= 0)
      goto bound_ok;
  }

  {
    const int err = WSAGetLastError();
    mgr[8549] = static_cast<unsigned>(err); // +34196
    const char *msg = format_wsa_error(ctx, static_cast<unsigned long>(err));
    if (!thegame::cfg.no_network_logs)
      lognf(static_cast<int>(s), "bind failed with errno %d: %s", err, msg);
    return 45;
  }

bound_ok: {
  sockaddr_storage bound{};
  int bound_len = sizeof(bound);
  if (getsockname(s, reinterpret_cast<sockaddr *>(&bound), &bound_len) < 0) {
    const int err = WSAGetLastError();
    mgr[8549] = static_cast<unsigned>(err);
    const char *msg = format_wsa_error(ctx, static_cast<unsigned long>(err));
    if (!thegame::cfg.no_network_logs)
      lognf(static_cast<int>(s), "getsockname() failed with errno %d: %s", err,
            msg);
    return 45;
  }
  logf("Local port: %hu", port);
  ctx[127] = 1;
  return 0;
}
}

} // namespace

bool ProudConnect::Socket::w_connect_3() {
  auto *base = reinterpret_cast<unsigned char *>(this);
  const SOCKET s = *reinterpret_cast<SOCKET *>(base + 20);

  sockaddr_in peer{};
  memcpy(&peer, base + 4, sizeof(sockaddr_in));

  if (peer.sin_family == AF_INET) {
    const u_short port = ntohs(peer.sin_port);
    in_addr was = peer.sin_addr;
    if (ServerOverride::remap_sockaddr_in(&peer) &&
        port == ServerOverride::kGameLegPort)
      logf("w_connect_3:27380 %s -> %s", inet_ntoa(was),
           inet_ntoa(peer.sin_addr));
    if (!thegame::cfg.no_network_logs)
      logns(static_cast<int>(s), inet_ntoa(peer.sin_addr), port);
  }

  return connect_syshandle(s, reinterpret_cast<sockaddr *>(&peer), 16) !=
         SOCKET_ERROR;
}

extern "C" int __fastcall ProudConnect::w_connect_2(int *out_zero,
                                                    SOCKET *out_socket,
                                                    int *ctx, int param4) {
  *out_socket = INVALID_SOCKET;
  *out_zero = 0;

  auto *const ctx_bytes = reinterpret_cast<unsigned char *>(ctx);
  void *const mgr = reinterpret_cast<void *>(*ctx);
  auto *const mgr_dwords = reinterpret_cast<unsigned *>(mgr);

  ConnectSockbuf sockbuf{};
  SOCKET sock = INVALID_SOCKET;

  if (open_socket(ctx, param4, reinterpret_cast<char *>(&sockbuf), &sock))
    return 0;

  if (!resolve_sockaddr(ctx, &sockbuf.name)) {
    const unsigned long err = GetLastError();
    const char *msg = format_wsa_error(ctx, err);
    if (!thegame::cfg.no_network_logs)
      lognf(static_cast<int>(sock),
            "sa_addr inet_ntop() failed with errno %lu: %s", err, msg);
    close_socket_ctx(ctx, sock);
    return 0;
  }

  memcpy(ctx_bytes + 0x40, ctx_bytes + 0x9E, 0x2Eu);
  if (!thegame::cfg.no_network_logs)
    logns(static_cast<int>(sock),
          reinterpret_cast<const char *>(ctx_bytes + 0x40),
          ntohs(sockbuf.name.sin_port));

  copy_ctx_fields(ctx);

  if (mgr_dwords[197])
    tcp_nodelay(sock, ctx, mgr_dwords);
  recvbuf_floor(sock);
  if (mgr_dwords[258])
    set_keepalive(mgr_dwords, sock);

  int skip_connect = 0;
  using SockCbFn = int(__cdecl *)(unsigned, SOCKET, unsigned);
  const auto cb = reinterpret_cast<SockCbFn>(mgr_dwords[91]);
  if (cb) {
    const int rc = cb(mgr_dwords[92], sock, 0);
    if (rc == 2)
      skip_connect = 1;
    else if (rc) {
      close_socket_ctx(ctx, sock);
      return 42;
    }
  }

  const int bind_rc = w_bind_3(ctx, sock, sockbuf.fields[0]);
  if (bind_rc) {
    close_socket_ctx(ctx, sock);
    return bind_rc;
  }

  set_nonblock(sock, 1);

  const unsigned tick = GetTickCount() / 1000u;
  *reinterpret_cast<unsigned *>(ctx_bytes + 0x204) = tick;
  *reinterpret_cast<unsigned *>(ctx_bytes + 0x208) = 0;

  if (*reinterpret_cast<int *>(ctx_bytes + 0x20C) > 1)
    timer_splay_connect(mgr, *reinterpret_cast<int *>(ctx_bytes + 0x210));

  if (skip_connect || *reinterpret_cast<int *>(ctx_bytes + 0x74) != 1) {
    *out_socket = sock;
    return 0;
  }

  const int conn_rc = connect_with_remap(
      sock, reinterpret_cast<sockaddr *>(&sockbuf.name), sockbuf.fields[3]);
  if (conn_rc != SOCKET_ERROR) {
    *out_socket = sock;
    return 0;
  }

  const int err = WSAGetLastError();
  if (connect_is_pending(err)) {
    *out_socket = sock;
    return 0;
  }

  const char *msg = format_wsa_error(ctx, static_cast<unsigned long>(err));
  if (!thegame::cfg.no_network_logs)
    lognf(static_cast<int>(sock), "Failed to connect to %s: %s",
          ctx_bytes + 0x40, msg);
  mgr_dwords[8549] = static_cast<unsigned>(err);
  close_socket_ctx(ctx, sock);
  return 0;
}
