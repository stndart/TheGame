#include "diagnostics/handlers.hpp"
#include "hook_manager.h"
#include "target_hooks.h"

#include <windows.h>

// Proud::IRmiProxy::RmiSend @ GAME RVA 0xD5C5E0 - the single chokepoint every
// outbound RMI passes through before MessageType_Rmi / 0x25 transport wrapping.
// __thiscall(this = ecx); stack args at function entry:
//   [esp+0x04] remotes (HostID*)   [esp+0x08] remoteCount (int)
//   [esp+0x0C] ctx                 [esp+0x10] msg (CMessage*)
//   [esp+0x14] RMIName (wchar*)    [esp+0x18] RMIId (u16 in a dword arg)
// SEH/GS prologue: 6A FF | 68 84 1D 51 01 | 64 A1 00 00 00 00 ...
// We relocate the first 7 bytes (push -1; push offset SEH_handler) into the
// stub and resume at 0xD5C5E7 (the `mov eax, fs:0`). Never resume at RVA+5.
// v1: log rmi_id + target count only (definitive). Body extraction is v2 once
// the CMessage layout is confirmed.

extern "C" void __cdecl log_c2s_rmi(unsigned rmi_id, unsigned remote_count) {
  char buf[96];
  const unsigned id = rmi_id & 0xFFFFu;
  wsprintfA(buf, "c2s_rmi id=0x%04X (%u) remotes=%u", id, id, remote_count);
  Diagnostics::emit_game_log(buf);
}

extern "C" void __declspec(naked) hook_pn_rmi_send() {
  __asm {
    pushad
    pushfd
    mov eax, [esp + 0x3C]   // RMIId      (entry esp+0x18, +0x24 for pushad+pushfd)
    mov ecx, [esp + 0x2C]   // remoteCount (entry esp+0x08, +0x24)
    push ecx
    push eax
    call log_c2s_rmi
    add esp, 8
    popfd
    popad
    push 0FFFFFFFFh         // replay stolen: push -1
    push 01511D84h          // replay stolen: push offset SEH_D5C5E0
    jmp [g_target_pn_rmi_send.return_addr]
  }
}

HookStub g_target_pn_rmi_send = {
    0xD5C5E0, (uint32_t)(uintptr_t)hook_pn_rmi_send, "hook_pn_rmi_send",
    {0},      false,                                 0xD5C5E7,
};
