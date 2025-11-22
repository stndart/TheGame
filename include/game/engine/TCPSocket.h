#pragma once

#include <cstdint>
#include <windows.h>

#include <winsock2.h>

#include "game/engine/WString.h"

class TCPSocket {
protected:
  TCPSocket();

  // this + 204 is char buffer, this + 200 is it's length
  char _padding[0x12C];
  SOCKET m_socketId; // Socket file descriptor

  bool m_blocking;                // Blocking flag
  struct sockaddr_in m_localAddr; // local Address/port
  struct sockaddr_in
      m_remoteAddr; // Address of the remote side of the connection

  int m_SendOffset;
  int m_ReceiveOffset;
  int m_ReceiveSize;

  char *m_pSendBuffer;
  char *m_pReceiveBuffer;

public:
  TCPSocket(uint16_t listenPort) {}; // TODO
  ~TCPSocket() {};                   // TODO

  int _Send();          // TODO // sub_CF3290, sub_D567F0, sub_D569C0
  int _SendTo();        // TODO // sub_D57590, sub_D577B0, sub_D57850
  int _Recv();          // TODO // sub_D56470
  int _RecvFrom();      // TODO // sub_D54BF0
  bool _Bind();         // TODO // sub_D581C0
  TCPSocket *_Accept(); // TODO
  void _Listen();       // TODO
  int _Connect();       // TODO
  void _Shutdown();     // TODO

  int Connect(WString wideHostname, int port);

public:
  // void sub_D56170(int, void *);
};