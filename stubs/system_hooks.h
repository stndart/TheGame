#pragma once
#include "hook_manager.h"

#include <winsock2.h>

extern SysHookStub g_ws2_send;
extern SysHookStub g_ws2_sendto;
extern SysHookStub g_ws2_wsasend;
extern SysHookStub g_ws2_wsasendto;
extern SysHookStub g_ws2_connect;

extern "C" int WSAAPI connect_syshandle(SOCKET s, const sockaddr *name,
                                        int namelen);
