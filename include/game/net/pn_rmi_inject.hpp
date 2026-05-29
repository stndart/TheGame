#pragma once

// In-process S2C game-RMI injection.
//
// The offline replay server cannot answer the client's lobby/room REQs: C2S is
// wrapped in transport 0x25 (reliable/compressed) and unreadable on the wire, so
// the server never knows a button was pressed. Instead we drive the client
// forward in-process: the C2S send hook (sub_65AEA0) tells us the exact REQ id
// the moment it is sent, and we run the matching S2C RES leaf handler directly on
// the receive/dispatch thread (the same thread the real dispatcher would use).
//
// RES leaves are bound as MSG_RETURN handler(MsgDelegateArg const* a1) and read
// the RMI body via *(u32*)(a1+8); the gating field is result(u16) at body+2
// (0 == success). See docs/proudnet-rmi-server-plan.md.

// Called from the C2S send hook (worker/send thread). Latches a pending
// injection when a REQ we know how to answer is observed. Latch-only: never runs
// a leaf here, so no game state is touched off the main thread.
void pn_inject_note_c2s_send(unsigned rmi_id);

// Main-thread pump points. The RES leaves drive RequestState/UI/scripting-VM
// work that MUST run on the main (frame) thread - the same thread CGameRoom's
// onPreProcess and the UI-variant VM run on. Running them from the ProudNet
// worker/drain thread races the frame loop and intermittently faults the VM
// (0xC0000005 in sub_1307B30). So we latch on send (any thread) and pump from
// the relevant IState::onPreProcess prologue hooks (main thread), BEFORE the
// original onPreProcess binds room data.

// room_list / lobby onPreProcess: runs the Create-Room transition RES.
void pn_inject_pump_lobby();

// CGameRoom::onPreProcess prologue: populates room data (first frame) then runs
// the start-match RES once Ready was pressed.
void pn_inject_pump_room();
