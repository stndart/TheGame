#include "diagnostics/handlers.hpp"
#include "ProudNet/RmiInject.hpp"
#include "hook_manager.h"
#include "target_hooks.h"

#include <windows.h>

// GAME application RMI proxy send chokepoints (NOT the framework proxy 0xD5C5E0,
// which only carries builtin heartbeat/ping ids 1001/1006/1019). Confirmed by IDA
// 2026-05-29 - see docs/plans/proudnet-rmi-server-plan.md "Game RMI proxy map".
//
//   sub_65AEA0  __thiscall(this=ecx)  (msg, len, rmiId:int16)  -- explicit id
//       16xxx band (account/lobby/room, proxy dword_1C1ABA0). 187 call sites.
//   sub_A0B290  __userpurge(a1=eax)   (msg, len)               -- id = *(u16*)msg
//       floor transmit; catches 15xxx-band inline senders (match/party,
//       proxy dword_1C1ABB0) that bypass the wrapper. sub_65AEA0 tail-calls it.
//
// Both have the plain non-SEH prologue 55 8B EC 83 E4 F8 (push ebp; mov ebp,esp;
// and esp,-8); steal 6 bytes, resume at target+6. Stack-arg offsets get +0x24 in
// the stub for pushad(0x20)+pushfd(0x04).

extern "C" void __cdecl log_c2s_rmi_proxy(unsigned rmi_id, unsigned len) {
  char buf[96];
  const unsigned id = rmi_id & 0xFFFFu;
  wsprintfA(buf, "c2s_grmi proxy id=0x%04X (%u) len=%u", id, id, len);
  Diagnostics::emit_game_log(buf);
  Proud::RmiInject::NoteC2sSend(id);
}

extern "C" void __cdecl log_c2s_rmi_floor(unsigned rmi_id, unsigned len) {
  char buf[96];
  const unsigned id = rmi_id & 0xFFFFu;
  wsprintfA(buf, "c2s_grmi floor id=0x%04X (%u) len=%u", id, id, len);
  Diagnostics::emit_game_log(buf);
}

// sub_65AEA0: rmiId (int16) at entry [esp+0x0C], len at [esp+0x08].
extern "C" void __declspec(naked) hook_pn_game_rmi_send() {
  __asm {
    pushad
    pushfd
    mov eax, [esp + 0x30]   // rmiId  (entry [esp+0x0C] + 0x24)
    movzx eax, ax
    mov ecx, [esp + 0x2C]   // len    (entry [esp+0x08] + 0x24)
    push ecx
    push eax
    call log_c2s_rmi_proxy
    add esp, 8
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_pn_game_rmi_send.return_addr]
  }
}

// sub_A0B290: msg (a2) at entry [esp+0x04], len at [esp+0x08]; rmi id = *(u16*)msg.
extern "C" void __declspec(naked) hook_pn_rmi_floor() {
  __asm {
    pushad
    pushfd
    mov eax, [esp + 0x28]   // msg    (entry [esp+0x04] + 0x24)
    movzx eax, word ptr [eax]  // rmi id = first u16 of msg (LE)
    mov ecx, [esp + 0x2C]   // len    (entry [esp+0x08] + 0x24)
    push ecx
    push eax
    call log_c2s_rmi_floor
    add esp, 8
    popfd
    popad
    push ebp
    mov ebp, esp
    and esp, 0FFFFFFF8h
    jmp [g_target_pn_rmi_floor.return_addr]
  }
}

HookStub g_target_pn_game_rmi_send = {
    0x65AEA0, (uint32_t)(uintptr_t)hook_pn_game_rmi_send, "hook_pn_game_rmi_send",
    {0},      false,                                      0x65AEA6,
};

HookStub g_target_pn_rmi_floor = {
    0xA0B290, (uint32_t)(uintptr_t)hook_pn_rmi_floor, "hook_pn_rmi_floor",
    {0},      false,                                  0xA0B296,
};
