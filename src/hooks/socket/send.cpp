#include "target_hooks.h"

#include <console.h>
#include <cstdint>
#include <minwindef.h>

#include "game/engine/String.h"
#include "game/net/socket_trace.hpp"

#include "WinSock2.h"

static bool should_hex_log(SOCKET sock) {
  const u_short port = SocketTrace::peer_port(sock);
  return port == SocketTrace::kEntryPort || port == SocketTrace::kGameLegPort;
}

void __cdecl handle_send_1(void *_this, char *buf, int len) {
  void **v3 = reinterpret_cast<void **>(reinterpret_cast<DWORD *>(_this)[2]);
  if (v3)
    _this = *v3;
  else
    _this = nullptr;
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  String s(buf, len);
  s.TruncateAtFirstOccurrence('\n');
  s.Truncate(s.GetLength() - 1);
  logf("handle_send_1: obj=%p, socket %p, buf='%s' at %p, len=%u", _this, sock,
       s.c_str(), buf, len);
}

void __cdecl handle_send_2(void *_this, char *buf, int len) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  if (should_hex_log(sock) && buf && len > 0)
    logn(SocketTrace::net_log_key(sock), static_cast<size_t>(len), buf, false);

  String s(buf, len);
  s.TruncateAtFirstOccurrence('\n');
  s.Truncate(s.GetLength() - 1);
  logf("handle_send_2: obj=%p, socket %p, buf='%s' at %p, len=%u", _this, sock,
       s.c_str(), buf, len);
}

void __cdecl handle_send_3(void *_this, int *a2) {
  SOCKET sock =
      *reinterpret_cast<SOCKET *>(reinterpret_cast<uintptr_t>(_this) + 300);

  WSABUF *lpBuffers = reinterpret_cast<WSABUF *>(a2 + 5);
  if (a2[3] > 100)
    lpBuffers = reinterpret_cast<WSABUF *>(a2[1]);

  size_t n = a2[2];
  logf("handle_send_3: conn=%p, socket %p, obj=%p, a2[0]=%u, someflag=%u, "
       "buffers[%u] at %p",
       _this, sock, a2, a2[0], a2[3], n, lpBuffers);
  for (size_t i = 0; i < n; ++i) {
    if (should_hex_log(sock))
      logn(SocketTrace::net_log_key(sock), lpBuffers[i].len, lpBuffers[i].buf,
           false);
  }
}

extern "C" void __declspec(naked) hook_send_1() {
  __asm {
    pushad
    mov eax, [esp + 0x28]
    mov ebx, [esp + 0x24]
    push eax
    push ebx
    push ecx
    call handle_send_1
    add esp, 12
    popad
    mov eax, [ecx+8]
    test eax, eax
    jmp [g_target_send_1.return_addr]
  }
}

extern "C" void __declspec(naked) hook_send_2() {
  __asm {
    pushad
    mov eax, [esp + 0x28]
    mov ebx, [esp + 0x24]
    push eax
    push ebx
    push ecx
    call handle_send_2
    add esp, 12
    popad
    push -1
    push 0x1511866
    jmp [g_target_send_2.return_addr]
  }
}

extern "C" void __declspec(naked) hook_send_3() {
  __asm {
    pushad
    mov eax, [esp + 0x24]
    push eax
    push ecx
    call handle_send_3
    add esp, 8
    popad
    push -1
    push 0x01511889
    jmp [g_target_send_3.return_addr]
  }
}

HookStub g_target_send_1 = {
    0xCF3290, (uint32_t)(uintptr_t)hook_send_1, "hook_send_1", {0}, false,
    0xCF3295,
};
HookStub g_target_send_2 = {
    0xD567F0, (uint32_t)(uintptr_t)hook_send_2, "hook_send_2", {0}, false,
    0xD567F7,
};
// Same RVA as TCPSocket::Send — enable hook_send_3 OR hook_TCPSocket_send, not both.
HookStub g_target_send_3 = {
    0xD569C0, (uint32_t)(uintptr_t)hook_send_3, "hook_send_3", {0}, false,
    0xD569C7,
};
