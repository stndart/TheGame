#pragma once

#include <cstdint>
#include <windows.h>

#include <winsock2.h>

#include "game/engine/WString.h"

class TCPSocket;
class NetCallbackInterface {
public:
  virtual ~NetCallbackInterface() = default;
  virtual void OnWarning(TCPSocket *socket, WString *warning); // vtable[1]
};

class TCPSocket {
protected:
  TCPSocket();

  // char _padding[0x12C];
  // SOCKET m_socketId; // Socket file descriptor

  // bool m_blocking;                // Blocking flag
  // struct sockaddr_in m_localAddr; // local Address/port
  // struct sockaddr_in
  //     m_remoteAddr; // Address of the remote side of the connection

  // int m_SendOffset;
  // int m_ReceiveOffset;
  // int m_ReceiveSize;

  // char *m_pSendBuffer;
  // char *m_pReceiveBuffer;

  BYTE _pad1[16];
  NetCallbackInterface *m_pCallback; // this + 16   (vtable pointer)
  BYTE _pad2[148];                   // [this + 20 .. + 168]
  WSAOVERLAPPED m_sendOverlapped;    // this + 168  (for async I/O)
  bool m_sendWarningFlag;            // this + 188 // m_isSending
  // this + 204 is char buffer, this + 200 is it's length
  BYTE _pad3[68];           // [this + 192 .. + 260]
  int m_sendOperationCount; // this + 260  (statistics)
  BYTE _pad4[34];           // [this + 264 .. + 298]
  bool m_sendPending;       // this + 298
  SOCKET m_socketId;        // this + 300

  struct MessageToSend {
    int dataSize;              // Must be > 0
    WSABUF *externalBufferPtr; // Used if bufferType > 100
    size_t bufferCount;        // Number of WSABUF structures
    int bufferType;            // Determines buffer source
    BYTE _pad;
    WSABUF inlineBufferPtr[1];
  };

public:
  TCPSocket(uint16_t listenPort) {}; // TODO
  ~TCPSocket() {};                   // TODO

  void log_structure();
  void log_message_structure(MessageToSend *message);

  int _Send();          // TODO // sub_CF3290, sub_D567F0
  int _SendTo();        // TODO // sub_D57590, sub_D577B0, sub_D57850
  int _Recv();          // TODO // sub_D56470
  int _RecvFrom();      // TODO // sub_D54BF0
  bool _Bind();         // TODO // sub_D581C0
  TCPSocket *_Accept(); // TODO
  void _Listen();       // TODO
  int _Connect();       // TODO
  void _Shutdown();     // TODO

  int Connect(WString wideHostname, int port); // sub_D56220
  int Send(MessageToSend *message);            // sub_D569C0

public:
  // void sub_D56170(int, void *);
};