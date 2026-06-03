#pragma once
#include "hook_manager.h"

#include <winsock2.h>

extern SysHookStub g_ws2_send;
extern SysHookStub g_ws2_sendto;
extern SysHookStub g_ws2_recv;
extern SysHookStub g_ws2_recvfrom;
extern SysHookStub g_ws2_wsasend;
extern SysHookStub g_ws2_wsasendto;
extern SysHookStub g_ws2_wsarecv;
extern SysHookStub g_ws2_wsarecvfrom;
extern SysHookStub g_ws2_connect;

extern "C" int WSAAPI send_syshandle(SOCKET s, const char *buf, int len,
                                     int flags);
extern "C" int WSAAPI sendto_syshandle(SOCKET s, const char *buf, int len,
                                       int flags, const sockaddr *to,
                                       int tolen);
extern "C" int WSAAPI recv_syshandle(SOCKET s, char *buf, int len, int flags);
extern "C" int WSAAPI recvfrom_syshandle(SOCKET s, char *buf, int len,
                                         int flags, sockaddr *from,
                                         int *fromlen);
extern "C" int WSAAPI wsasend_syshandle(SOCKET s, LPWSABUF bufs,
                                        DWORD buffer_count, LPDWORD bytes_sent,
                                        DWORD flags, LPWSAOVERLAPPED overlapped,
                                        LPWSAOVERLAPPED_COMPLETION_ROUTINE completion);
extern "C" int WSAAPI wsasendto_syshandle(SOCKET s, LPWSABUF bufs,
                                         DWORD buffer_count, LPDWORD bytes_sent,
                                         DWORD flags, const sockaddr *to,
                                         int tolen, LPWSAOVERLAPPED overlapped,
                                         LPWSAOVERLAPPED_COMPLETION_ROUTINE completion);
extern "C" int WSAAPI wsarecv_syshandle(SOCKET s, LPWSABUF bufs,
                                        DWORD buffer_count, LPDWORD bytes_recvd,
                                        LPDWORD flags, LPWSAOVERLAPPED overlapped,
                                        LPWSAOVERLAPPED_COMPLETION_ROUTINE completion);
extern "C" int WSAAPI wsarecvfrom_syshandle(
    SOCKET s, LPWSABUF bufs, DWORD buffer_count, LPDWORD bytes_recvd,
    LPDWORD flags, sockaddr *from, LPINT fromlen, LPWSAOVERLAPPED overlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion);
extern "C" int WSAAPI connect_syshandle(SOCKET s, const sockaddr *name,
                                        int namelen);
