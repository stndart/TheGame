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
sub_t sub_D56170 = (sub_t)0x00D56170; // original address
// skips if error is WSA_IO_PENDING (997) or WSAEWOULDBLOCK (10035)

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
  if (LOG_CONNECT)
    logf("TCPSocket::Connect at %p to %ls:%u", this, wideHostname.c_str(),
         port);

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