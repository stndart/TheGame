#pragma once

#include <cstdint>
#include <windows.h>

#include <winsock2.h>

#include "game/engine/WString.h"

class TCPSocket {
protected:
  TCPSocket();

  SOCKET m_socketId;              // Socket file descriptor
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

  int Send();          // TODO
  int Receive();       // TODO
  int ReceiveFrom();   // TODO
  bool Bind();         // TODO
  TCPSocket *Accept(); // TODO
  void Listen();       // TODO
  int Connect_();      // TODO
  void Shutdown();     // TODO

  int Connect(WString wideHostname, int port);

public:
  // void sub_D56170(int, void *);
};