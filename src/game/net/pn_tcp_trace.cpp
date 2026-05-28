#include "game/net/pn_tcp_trace.hpp"

#include "diagnostics/handlers.hpp"
#include "game/net/proud_frame_parse.hpp"
#include "game/net/socket_trace.hpp"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <windows.h>

namespace {

std::ofstream g_pn_tcp_file;
constexpr size_t kMaxFramesPerLine = 32;

void ensure_log_open() {
  if (g_pn_tcp_file.is_open())
    return;
  g_pn_tcp_file.open(PnTcpTrace::kLogFileName, std::ios::app);
  if (g_pn_tcp_file.is_open())
    g_pn_tcp_file << "# proudnet-tcp headers (tid port dir chunk frames)\n";
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
    out << "{pl=" << f.payload_len << ",op=0x" << std::hex;
    out.width(2);
    out.fill('0');
    out << static_cast<unsigned>(f.inner.opcode) << std::dec;
    if (f.inner.has_rmi) {
      out << ",rmi=0x" << std::hex;
      out.width(4);
      out.fill('0');
      out << f.inner.rmi_id << std::dec;
    }
    out << ",body=" << f.inner.body_len << "}";
  }
  out << "]";
}

void log_line(SOCKET sock, u_short port, const char *dir, size_t chunk_len,
              const ProudFrameParse::ParsedFrame *frames, size_t frame_count,
              size_t incomplete_tail) {
  ensure_log_open();
  if (!g_pn_tcp_file.is_open())
    return;

  const DWORD tid = GetCurrentThreadId();
  g_pn_tcp_file << "tid=" << tid << " port=" << port << " dir=" << dir
                << " sock=" << static_cast<unsigned long long>(sock)
                << " chunk=" << chunk_len;
  if (incomplete_tail)
    g_pn_tcp_file << " incomplete=" << incomplete_tail;
  append_frames_text(g_pn_tcp_file, frames, frame_count);
  g_pn_tcp_file << "\n";
  g_pn_tcp_file.flush();

#ifdef THEGAME_PN_TCP_PIPE
  Diagnostics::PnTcpFrameHeader pipe_frames[kMaxFramesPerLine];
  for (size_t i = 0; i < frame_count; ++i) {
    pipe_frames[i].payload_len =
        static_cast<unsigned>(frames[i].payload_len);
    pipe_frames[i].opcode = frames[i].inner.opcode;
    pipe_frames[i].rmi_id = frames[i].inner.rmi_id;
    pipe_frames[i].body_len = static_cast<unsigned>(frames[i].inner.body_len);
    pipe_frames[i].has_rmi = frames[i].inner.has_rmi ? 1 : 0;
  }
  Diagnostics::emit_proudnet_tcp(
      tid, port, dir,
      static_cast<unsigned long long>(sock),
      chunk_len, pipe_frames, frame_count, incomplete_tail);
#endif
}

} // namespace

namespace PnTcpTrace {

void log_connect(SOCKET sock, const char *addr, u_short port) {
  if (!SocketTrace::is_pn_track_port(port))
    return;

  ensure_log_open();
  if (!g_pn_tcp_file.is_open())
    return;

  const DWORD tid = GetCurrentThreadId();
  g_pn_tcp_file << "tid=" << tid << " event=connect port=" << port << " sock="
                << static_cast<unsigned long long>(sock)
                << " addr=" << (addr ? addr : "?") << "\n";
  g_pn_tcp_file.flush();

#ifdef THEGAME_PN_TCP_PIPE
  Diagnostics::emit_proudnet_tcp_connect(
      tid, static_cast<unsigned long long>(sock),
      addr, port);
#endif
}

void log_chunk(SOCKET sock, const void *data, size_t len, bool inbound) {
  if (!data || len == 0)
    return;

  u_short port = SocketTrace::pn_log_port(sock);
  if (!SocketTrace::is_pn_track_port(port))
    port = SocketTrace::fast_socket_log_port(SocketTrace::last_fast_socket_ctx());

  if (port == 0 && sock == INVALID_SOCKET &&
      !SocketTrace::last_fast_socket_ctx())
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

  log_line(sock, port, inbound ? "rx" : "tx", len, frames, frame_count,
           incomplete);
}

void close_log_file() {
  if (g_pn_tcp_file.is_open())
    g_pn_tcp_file.close();
}

} // namespace PnTcpTrace
