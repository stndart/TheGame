#include "game/engine/TCPSocket.h"
#include "diagnostics/handlers.hpp"
#include "game/net/pn_growable.hpp"
#include "game/net/pn_socket_error.hpp"
#include "game/net/pn_tcp_trace.hpp"
#include "game/net/socket_trace.hpp"
#include "game/server_override.hpp"

#include <string>
#include <winerror.h>
#include <winsock2.h>

#include "game/engine/MemoryDefines.h"
#include "game/engine/StringConverters.h"
// #include "game/engine/WString.h"

inline std::string to_ip_string(int addr) {
  unsigned char *caddr = reinterpret_cast<unsigned char *>(&addr);
  std::string res = "";
  res += std::to_string(caddr[0]);
  res += ".";
  res += std::to_string(caddr[1]);
  res += ".";
  res += std::to_string(caddr[2]);
  res += ".";
  res += std::to_string(caddr[3]);
  return res;
}

int TCPSocket::Connect(WString wideHostname, int port) {
  if (!this)
    return WSAEFAULT;

  if (port == 7000) {
    Diagnostics::emit_game_state("connecting_to_server");
  }
  if (SocketTrace::is_pn_track_port(static_cast<u_short>(port)))
    SocketTrace::track_connect(m_socketId, static_cast<u_short>(port));

  // Convert wide char hostname to multibyte
  MultiByteHolder multibyteHolder;
  multibyteHolder.ConvertWideToMultiByte(wideHostname.c_str(), CP_THREAD_ACP);
  std::string host = multibyteHolder.current_ptr ? multibyteHolder.current_ptr : "";
  const std::string override_ip = ServerOverride::remap_host(host.c_str());
  if (!override_ip.empty()) {
    logf("TCPSocket::Connect remap %s -> %s", host.c_str(), override_ip.c_str());
    host = override_ip;
  }

  // Clean up the temporary multibyte buffer
  if (multibyteHolder.current_ptr != multibyteHolder.inline_buf)
    EE_FREE(multibyteHolder.current_ptr);

  // Set up socket address structure
  sockaddr_in serverAddr;
  memset(&serverAddr.sin_port, 0, 14); // Zero out port and address
  serverAddr.sin_family = AF_INET;

  // Try to interpret as IP address first
  serverAddr.sin_addr.s_addr = inet_addr(host.c_str());
  const unsigned long remapped = ServerOverride::remap_ipv4(serverAddr.sin_addr.s_addr);
  if (remapped)
    serverAddr.sin_addr.s_addr = remapped;

  // If not an IP address, try DNS resolution
  if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
    hostent *hostEntry = gethostbyname(host.c_str());

    if (!hostEntry) {
      int error = WSAGetLastError();
      pn::socket_report_error(this, error, nullptr);
      return error;
    }
    serverAddr.sin_addr.s_addr = *(ULONG *)hostEntry->h_addr_list[0];
    const unsigned long remapped_dns =
        ServerOverride::remap_ipv4(serverAddr.sin_addr.s_addr);
    if (remapped_dns)
      serverAddr.sin_addr.s_addr = remapped_dns;
  }

  // Set port and attempt connection
  serverAddr.sin_port = htons(static_cast<u_short>(port));
  ServerOverride::remap_sockaddr_in(&serverAddr);
  std::string ipstr =
      to_ip_string(*reinterpret_cast<int *>(&serverAddr.sin_addr));

  logf("Connecting socket %p to addr %s:%u", m_socketId, ipstr.c_str(), port);
  if (port == ServerOverride::kGameLegPort)
    logf("TCPSocket:27380 target %s:%u", ipstr.c_str(), port);

  PnTcpTrace::log_connect(m_socketId, ipstr.c_str(),
                          static_cast<u_short>(port));

  if (connect(m_socketId, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    int error = WSAGetLastError();
    pn::socket_report_error(this, error, nullptr);
    if (error != WSAEWOULDBLOCK && error != WSA_IO_PENDING)
      logf("Error connecting to %s: %u", ipstr.c_str(), error);
    return error;
  }

  return 0;
}

int TCPSocket::Send(TCPSocket::MessageToSend *message) {
  // Check if we need to send a warning
  if (m_sendWarningFlag) {
    logf("TCPSocket::Send: IssueSend duplicated sock=%p", m_socketId);
  }

  // Validate send parameters
  if (message->dataSize <= 0) {
    return WSAEINVAL; // 10022 = Invalid argument
  }

  DWORD bytesSent = 0;
  this->m_sendWarningFlag = false;
  this->m_sendWarningFlag = true;

  // Range check for buffer count
  if (((uint64_t)message->bufferCount + 0x80000000) >> 32) {
    pn::growable::throw_send_buffer_count_out_of_range();
  }

  // Use WSASend for overlapped I/O
  int lastError;

  while (true) {
    WSABUF *lpBuffers;
    if (message->bufferType > 100) { // message->bufferType > 100
      lpBuffers = message->externalBufferPtr;
    } else {
      // Use inline buffers in sendParams structure
      lpBuffers = message->inlineBufferPtr;
    }

    // Perform overlapped send (PnTcpTrace on WSASend IAT hook)
    if (WSASend(this->m_socketId, lpBuffers, message->bufferCount, &bytesSent,
                0, &m_sendOverlapped, NULL) >= 0) {
      // Success
      ++this->m_sendOperationCount;
      this->m_sendPending = true;
      return 0;
    }

    lastError = WSAGetLastError();

    if (lastError == ERROR_IO_PENDING) { // 997 = Operation in progress
      ++this->m_sendOperationCount;
      this->m_sendPending = false;
      return 0;
    }

    if (lastError != WSAEINTR) { // 10004 = Interrupted
      break;
    }

    pn::growable::note_wsa_interrupt_retry();
  }

  // Handle other errors
  this->m_sendWarningFlag = false;
  pn::socket_report_error(this, lastError, nullptr);

  return lastError;
}
