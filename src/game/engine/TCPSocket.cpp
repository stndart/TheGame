#include "game/engine/TCPSocket.h"

#include <string>
#include <winerror.h>
#include <winsock2.h>

#include "game/engine/MemoryDefines.h"
#include "game/engine/String.h"
#include "game/engine/StringConverters.h"
// #include "game/engine/WString.h"

#define LOG_CONNECT 1

using sub_t = int(__thiscall *)(void *thisptr, int arg1, void *arg2);
sub_t sub_D56170 = (sub_t)0xD56170; // original address
// skips if error is WSA_IO_PENDING (997) or WSAEWOULDBLOCK (10035)

using fmt_t = void(__cdecl *)(int a1);
fmt_t fmt_out_of_range_error = (fmt_t)0xCEFB40;

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

void TCPSocket::log_structure() {
  logf("this addr 0x%p, size %u", this, sizeof(TCPSocket));
  logf("m_pCallback offset %u, size %u",
       (int32_t)(&m_pCallback) - (int32_t)this, sizeof(NetCallbackInterface *));
  logf("m_sendOverlapped offset %u, size %u",
       (int32_t)(&m_sendOverlapped) - (int32_t)this, sizeof(m_sendOverlapped));
  logf("m_sendWarningFlag offset %u, size %u",
       (int32_t)(&m_sendWarningFlag) - (int32_t)this,
       sizeof(m_sendWarningFlag));
  logf("m_sendOperationCount offset %u, size %u",
       (int32_t)(&m_sendOperationCount) - (int32_t)this,
       sizeof(m_sendOperationCount));
  logf("m_sendPending offset %u, size %u",
       (int32_t)(&m_sendPending) - (int32_t)this, sizeof(m_sendPending));
  logf("m_socketId offset %u, size %u", (int32_t)(&m_socketId) - (int32_t)this,
       sizeof(m_socketId));
}

void TCPSocket::log_message_structure(TCPSocket::MessageToSend *message) {
  logf("Message layout at %p", message);
  logf("dataSize with offset %u",
       (int32_t)&message->dataSize - (int32_t)message);
  logf("externalBufferPtr with offset %u",
       (int32_t)&message->externalBufferPtr - (int32_t)message);
  logf("bufferCount with offset %u",
       (int32_t)&message->bufferCount - (int32_t)message);
  logf("bufferType with offset %u",
       (int32_t)&message->bufferType - (int32_t)message);
  logf("inlineBufferPtr with offset %u",
       (int32_t)message->inlineBufferPtr - (int32_t)message);
}

int TCPSocket::Connect(WString wideHostname, int port) {
  if (LOG_CONNECT)
    logf("TCPSocket::Connect at %p to %ls:%u", this, wideHostname.c_str(),
         port);

  // log_structure();

  // Convert wide char hostname to multibyte
  MultiByteHolder multibyteHolder;
  multibyteHolder.ConvertWideToMultiByte(wideHostname.c_str(), CP_THREAD_ACP);
  String name = multibyteHolder.current_ptr;

  // Clean up the temporary multibyte buffer
  if (multibyteHolder.current_ptr != multibyteHolder.inline_buf)
    EE_FREE(multibyteHolder.current_ptr);

  // Set up socket address structure
  sockaddr_in serverAddr;
  memset(&serverAddr.sin_port, 0, 14); // Zero out port and address
  serverAddr.sin_family = AF_INET;

  // Try to interpret as IP address first
  serverAddr.sin_addr.s_addr = inet_addr(name.c_str());

  // If not an IP address, try DNS resolution
  if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
    hostent *hostEntry = gethostbyname(name.c_str());

    if (!hostEntry) {
      // DNS resolution failed
      int error = WSAGetLastError();
      sub_D56170(this, error, (void *)0x15DC0C0); // Report DNS error
      return error;
    }
    serverAddr.sin_addr.s_addr = *(ULONG *)hostEntry->h_addr_list[0];
  }

  // Set port and attempt connection
  serverAddr.sin_port = htons(port);
  std::string ipstr =
      to_ip_string(*reinterpret_cast<int *>(&serverAddr.sin_addr));

  if (LOG_CONNECT)
    logf("Connecting socket %p to addr %s:%u", m_socketId, ipstr.c_str(), port);

  logns(m_socketId, ipstr.c_str(), port);

  if (connect(m_socketId, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
      SOCKET_ERROR) {
    int error = WSAGetLastError();
    sub_D56170(this, error,
               (void *)0x15DC0F8); // Report connection error
    if (LOG_CONNECT) {
      if (error == WSAEWOULDBLOCK || error == WSA_IO_PENDING)
        logf("Error connecting to %s: %u (expected)", ipstr.c_str(), error);
      else
        logf("Error connecting to %s: %u", ipstr.c_str(), error);
    }
    return error;
  }

  return 0;
}

int TCPSocket::Send(TCPSocket::MessageToSend *message) {
  if (LOG_CONNECT)
    logf("TCPSocket::Send at %p with %p and bufcnt %u", this, message,
         message->bufferCount);

  // Check if we need to send a warning
  if (m_sendWarningFlag) {
    WString warningMsg = L"WARNING: IssueSend is duplicated!";
    m_pCallback->OnWarning(this, &warningMsg);
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
    // Throw if buffer count is too large
    fmt_out_of_range_error(0x15DC310);
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

    if (LOG_CONNECT)
      logf("handle TCPSocket::Send: obj=%p, socket %p, datasize=%u, "
           "buffertype=%u, lpBuffers[%u] at %p",
           this, this->m_socketId, message->dataSize, message->bufferType,
           message->bufferCount, lpBuffers);

    for (size_t i = 0; i < message->bufferCount; ++i) {
      String s(lpBuffers[i].buf, lpBuffers[i].len);
      s.TruncateAtFirstOccurrence('\n');
      s.Truncate(s.GetLength() - 1);
      logn(this->m_socketId, lpBuffers[i].len, lpBuffers[i].buf);
    }

    // Perform overlapped send
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

    // For interrupted calls, increment global counter and retry
    unsigned long g_interruptCount =
        *reinterpret_cast<unsigned long *>(0x180DF00);
    InterlockedIncrement(&g_interruptCount);
  }

  // Handle other errors
  this->m_sendWarningFlag = false;
  sub_D56170(this, lastError, (void *)0x15DC360); // Report send error

  return lastError;
}