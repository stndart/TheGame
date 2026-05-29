#pragma once

// In-process S2C game-RMI injection (game stubs/proxies, not ProudNet transport).
//
// The offline replay server cannot answer the client's lobby/room REQs: C2S is
// wrapped in transport 0x25 (reliable/compressed) and unreadable on the wire, so
// the server never knows a button was pressed. Instead we drive the client
// forward in-process: the C2S send hook (sub_65AEA0) tells us the exact REQ id
// the moment it is sent, and we run the matching S2C RES leaf handler directly.
//
// RES leaves are bound as MSG_RETURN handler(MsgDelegateArg const* a1) and read
// the RMI body via *(u32*)(a1+8); the gating field is result(u16) at body+2
// (0 == success). See docs/plans/proudnet-rmi-server-plan.md.
//
// Disable at compile time: CMake -DTHEGAME_DISABLE_RMI_INJECT=ON (preset
// debug-wire). At runtime only: env THEGAME_DISABLE_RMI_INJECT=1 on GAME.exe.

namespace Rmi {

// Called from the C2S send hook (worker/send thread). Latches a pending injection
// when a REQ we know how to answer is observed. Latch-only: never runs a leaf
// here, so no game state is touched off the main thread.
void NoteC2sSend(unsigned rmi_id);

// Main-thread pump points (IState::onPreProcess prologue hooks).
void PumpLobby();
void PumpRoom();

} // namespace Rmi
