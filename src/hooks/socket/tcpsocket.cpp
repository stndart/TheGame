#include "target_hooks.h"

#include <game/engine/TCPSocket.h>

extern "C" void __declspec(naked) hook_TCPSocket_connect() {
  __asm {
    jmp TCPSocket::Connect
  }
}

extern "C" void __declspec(naked) hook_TCPSocket_send() {
  __asm {
    jmp TCPSocket::Send
  }
}

HookStub g_target_TCPSocket_connect = {
    0xD56220,
    (uint32_t)(uintptr_t)hook_TCPSocket_connect,
    "hook_TCPSocket_connect",
    {0},
    false,
    0xD56227,
};

HookStub g_target_TCPSocket_send = {
    0xD569C0,
    (uint32_t)(uintptr_t)hook_TCPSocket_send,
    "hook_TCPSocket_send",
    {0},
    false,
    0xD569C7,
};
