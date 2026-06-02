#include "ProudNet/TcpTrace.hpp"

#include <cstdio>
#include <iomanip>
#include <sstream>
#include <windows.h>

#include <fmt/format.h>

#include "diagnostics/handlers.hpp"
#include "game/net/proud_frame_parse.hpp"
#include "game/net/socket_trace.hpp"
#include "thegame/config.hpp"
#include "thegame/log.hpp"

namespace {

constexpr size_t kMaxFramesPerLine = 32;

void append_raw_preview(std::ostream &out, const uint8_t *data, size_t len) {
  constexpr size_t kPreview = 24;
  const size_t n = len < kPreview ? len : kPreview;
  if (n == 0)
    return;
  out << " raw=";
  for (size_t i = 0; i < n; ++i) {
    out << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<unsigned>(data[i]) << std::dec;
  }
  if (len > n)
    out << "...";
}

u_short resolve_log_port(SOCKET sock, void *fast_socket_ctx) {
  u_short port = SocketTrace::pn_log_port(sock);
  if (SocketTrace::is_pn_track_port(port))
    return port;

  if (fast_socket_ctx) {
    port = SocketTrace::fast_socket_log_port(fast_socket_ctx);
    if (SocketTrace::is_pn_track_port(port))
      return port;
  }

  void *last = SocketTrace::last_fast_socket_ctx();
  if (last) {
    port = SocketTrace::fast_socket_log_port(last);
    if (SocketTrace::is_pn_track_port(port))
      return port;
  }

  if (SocketTrace::is_tracked_pn_socket(sock))
    return SocketTrace::pn_log_port(sock);

  return port;
}

bool should_log_pn_chunk(SOCKET sock, u_short port, void *fast_socket_ctx) {
  if (SocketTrace::is_pn_track_port(port))
    return true;
  if (SocketTrace::is_tracked_pn_socket(sock))
    return true;
  if (fast_socket_ctx)
    return true;
  if (SocketTrace::last_fast_socket_ctx())
    return true;
  return false;
}

void append_frames_text(std::ostream &out,
                        const ProudFrameParse::ParsedFrame *frames,
                        size_t frame_count) {
  out << " frames=";
  if (frame_count == 0) {
    out << "none";
    return;
  }
  out << "[";
  for (size_t i = 0; i < frame_count; ++i) {
    if (i)
      out << ",";
    const auto &f = frames[i];
    out << "{op=0x" << std::hex;
    out.width(2);
    out.fill('0');
    out << static_cast<unsigned>(f.inner.opcode) << std::dec;
    if (f.inner.has_rmi) {
      out << ",rmi=0x" << std::hex;
      out.width(4);
      out.fill('0');
      out << f.inner.rmi_id << std::dec;
    }
    out << ",body[" << f.inner.body_len << "]}";
  }
  out << "]";
}

const uint8_t kOpKeepalive = 0x1C;

void log_line(SOCKET sock, u_short port, const char *dir, size_t chunk_len,
              const uint8_t *raw, const ProudFrameParse::ParsedFrame *frames,
              size_t frame_count, size_t incomplete_tail) {
  if (thegame::cfg.no_proud_logs)
    return;

  if (thegame::cfg.silent_keepalive) {
    if (frame_count == 1 && frames[0].inner.opcode == kOpKeepalive)
      return;
  }

  const DWORD tid = GetCurrentThreadId();
  std::ostringstream line;
  line << fmt::format("{} :{} tid={} sock={} chunk={}", dir, port, tid, sock,
                      chunk_len);
  if (incomplete_tail)
    line << " incomplete=" << incomplete_tail;
  append_frames_text(line, frames, frame_count);
  if (frame_count == 0 && raw && chunk_len)
    append_raw_preview(line, raw, chunk_len);

  thegame::logpln(line.str().c_str());

  if (thegame::cfg.pipes) {
    Diagnostics::PnTcpFrameHeader pipe_frames[kMaxFramesPerLine];
    for (size_t i = 0; i < frame_count; ++i) {
      pipe_frames[i].payload_len = static_cast<unsigned>(frames[i].payload_len);
      pipe_frames[i].opcode = frames[i].inner.opcode;
      pipe_frames[i].rmi_id = frames[i].inner.rmi_id;
      pipe_frames[i].body_len = static_cast<unsigned>(frames[i].inner.body_len);
      pipe_frames[i].has_rmi = frames[i].inner.has_rmi ? 1 : 0;
    }
    Diagnostics::emit_proudnet_tcp(
        tid, port, dir, static_cast<unsigned long long>(sock), chunk_len,
        pipe_frames, frame_count, incomplete_tail);
  }
}

} // namespace

namespace Proud {
namespace TcpTrace {

void log_connect(SOCKET sock, const char *addr, u_short port) {
  if (!SocketTrace::is_pn_track_port(port))
    return;
  thegame::logpns(static_cast<int>(sock), addr, port);
}

void log_chunk(SOCKET sock, const void *data, size_t len, bool inbound,
               void *fast_socket_ctx) {
  if (!data || len == 0)
    return;

  const u_short port = resolve_log_port(sock, fast_socket_ctx);
  if (!should_log_pn_chunk(sock, port, fast_socket_ctx))
    return;

  if (SocketTrace::is_pn_track_port(port) && sock != INVALID_SOCKET &&
      !SocketTrace::is_tracked_pn_socket(sock))
    SocketTrace::track_connect(sock, port);

  const auto *bytes = static_cast<const uint8_t *>(data);
  ProudFrameParse::ParsedFrame frames[kMaxFramesPerLine];
  const size_t frame_count =
      ProudFrameParse::scan_frames(bytes, len, frames, kMaxFramesPerLine);

  size_t parsed_wire = 0;
  for (size_t i = 0; i < frame_count; ++i)
    parsed_wire += frames[i].wire_bytes;

  const size_t incomplete =
      ProudFrameParse::trailing_unparsed(bytes, len, parsed_wire);

  log_line(sock, port, inbound ? "rx" : "tx", len, bytes, frames, frame_count,
           incomplete);
}

void close_log_file() {
  // Closed via thegame::close_logs() in DllMain.
}

} // namespace TcpTrace
} // namespace Proud
