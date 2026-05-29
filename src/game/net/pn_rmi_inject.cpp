#include "game/net/pn_rmi_inject.hpp"

#include "diagnostics/handlers.hpp"

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace {

std::uintptr_t game_va(const std::uint32_t va) {
  const std::uintptr_t delta =
      reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr)) - 0x400000u;
  return static_cast<std::uintptr_t>(va) + delta;
}

// C2S REQ ids (proxy band, sub_65AEA0 explicit id) we trigger on.
constexpr unsigned kReqCreateRoom = 0x3F30; // Custom-match Create Room
constexpr unsigned kReqRoomReady = 0x3F2B;  // waiting-room Ready toggle

// S2C RES leaf handlers (RVA == IDA EA). Signature: int __stdcall(MsgDelegateArg
// const* a1); body = *(u32*)(a1+8). gating result(u16) at body+2 (0 == success).
//
// 0x3F30 (sub_437160): on result==0 requests scene 9 (CGameRoom).
constexpr std::uint32_t kLeafCreateRoomRes = 0x437160;
// 0x3F3D (sub_43D9B0): game-start. On result==0, if flt_1C25E2C set, copies
// body+4 (wstring) via sub_65C1C0, stores body+0x28 -> flt_1C25E2C[0x210], then
// requests scene 11 (CGamePlay -> CBasePlayLoading map load).
constexpr std::uint32_t kLeafStartRes = 0x43D9B0;
// 0x3ED4 (sub_4BB560): room-enter ACK / COMPACT member list. body+2 int count,
// then count * 32B records at body+6. Fills the compact member map (off_1C28684
// keyed by record+0 = account_id, plus key-set dword_1C2EA08). NOTE: this is NOT
// the map the room UI VM reads (see 0x3ED8); kept for real-server parity.
constexpr std::uint32_t kLeafRoomEnterRes = 0x4BB560;
// 0x3ED8 (sub_4BB370): room-enter ACK / UI member list (168B records). body+2
// int count, then count * 0xA8 records at body+6. Fills dword_1C28898 which
// CGameRoom::onPreProcess -> sub_442F90 -> sub_8DCBF0(slot) actually binds. This
// is the map whose emptiness faults the UI-variant VM (sub_1307B30). A coherent
// slot-0 record needs validity bytes set (see inject_room_members_res()).
constexpr std::uint32_t kLeafRoomMembersRes = 0x4BB370;
constexpr std::size_t kUiMemberStride = 0xA8; // 168B record (sub_8DCBB0 +=42)

// Local player account id our connect inject hands the client (server uses 1).
constexpr std::uint32_t kLocalAccountId = 1;

bool rmi_inject_disabled() {
#if defined(THEGAME_DISABLE_RMI_INJECT)
  return true;
#else
  static int cached = -1;
  if (cached < 0) {
    char buf[4];
    cached = (GetEnvironmentVariableA("THEGAME_DISABLE_RMI_INJECT", buf,
                                       sizeof(buf)) > 0)
                 ? 1
                 : 0;
  }
  return cached == 1;
#endif
}

volatile LONG g_pending_create_room = 0;
volatile LONG g_pending_populate = 0;
volatile LONG g_pending_start = 0;
volatile LONG g_start_fired = 0;

// Invoke an RES leaf with a flat body buffer. The leaf only dereferences
// *(u32*)(a1+8) -> body; no other MsgDelegateArg field is read on these paths,
// so a zeroed arg with the body pointer at +8 is sufficient.
void call_res_leaf(std::uint32_t leaf_rva, const void *body) {
  void *arg[8] = {nullptr};
  arg[2] = const_cast<void *>(body); // a1 + 8 -> body
  using LeafFn = int(__stdcall *)(void *);
  const auto fn = reinterpret_cast<LeafFn>(game_va(leaf_rva));
  fn(arg);
}

void inject_create_room_res() {
  // body+0: pad (ignored), body+2: result u16 == 0 -> success -> RequestState(9).
  unsigned char body[8] = {0};
  char msg[96];
  wsprintfA(msg, "inject: create-room RES 0x3F30 result=0 -> GameRoom (tid=%lu)",
            GetCurrentThreadId());
  Diagnostics::emit_game_log(msg);
  call_res_leaf(kLeafCreateRoomRes, body);
}

void put_u32(unsigned char *p, std::size_t off, std::uint32_t v) {
  p[off + 0] = static_cast<unsigned char>(v & 0xFF);
  p[off + 1] = static_cast<unsigned char>((v >> 8) & 0xFF);
  p[off + 2] = static_cast<unsigned char>((v >> 16) & 0xFF);
  p[off + 3] = static_cast<unsigned char>((v >> 24) & 0xFF);
}

// Populate CGameRoom's COMPACT member map (off_1C28684). body+2 count=1, one 32B
// record whose key (record+0) is the local account id. The UI does not read this
// map directly; injected for real-server parity / coherence.
void inject_room_enter_res() {
  unsigned char body[6 + 32] = {0};
  put_u32(body, 2, 1);                       // count = 1
  put_u32(body, 6 + 0x00, kLocalAccountId);  // record+0: account_id (map key)
  // record+4 slot, +8 team, +0x10 name-handle left 0: name binds off the char DB
  // (dword_1C15354), not this record.
  Diagnostics::emit_game_log(
      "inject: room-enter RES 0x3ED4 count=1 (compact map, acct=1)");
  call_res_leaf(kLeafRoomEnterRes, body);
}

// Populate the UI member map (dword_1C28898) the room script actually binds. One
// 168B slot-0 record with the validity fields sub_8DCBF0 / sub_4F8670 gate on:
//   record+0    = slot index (0)
//   record+4    = 1   (sub_8F98B0 present path)
//   record+0x14 = 1   (sub_4F8670 checks == 1)
//   record+0x28 = 1   (sub_8DCBF0 validity byte; else +0x2C == 1)
//   record+0x20 = 0   (isLockIn QWORD: not locked)
// Field semantics are inferred (subagent [S]); validate at runtime.
void inject_room_members_res() {
  unsigned char body[6 + kUiMemberStride] = {0};
  put_u32(body, 2, 1);             // count = 1
  const std::size_t r = 6;         // record base
  put_u32(body, r + 0x00, 0);      // slot index 0 (key for sub_8DCBF0(0))
  put_u32(body, r + 0x04, 1);      // present path
  put_u32(body, r + 0x14, 1);      // sub_4F8670 == 1
  body[r + 0x28] = 1;              // sub_8DCBF0 validity
  Diagnostics::emit_game_log(
      "inject: room-members RES 0x3ED8 count=1 slot=0 (UI map)");
  call_res_leaf(kLeafRoomMembersRes, body);
}

void inject_start_res() {
  // 0x40 zeroed: result(+2)=0, empty wstring(+4), mode(+38)=0, param(+40)=0.
  unsigned char body[0x40] = {0};
  char msg[96];
  wsprintfA(msg, "inject: start RES 0x3F3D result=0 -> CGamePlay (tid=%lu)",
            GetCurrentThreadId());
  Diagnostics::emit_game_log(msg);
  call_res_leaf(kLeafStartRes, body);
}

} // namespace

void pn_inject_note_c2s_send(unsigned rmi_id) {
  if (rmi_inject_disabled())
    return;
  const unsigned id = rmi_id & 0xFFFFu;
  if (id == kReqCreateRoom) {
    InterlockedExchange(&g_pending_create_room, 1);
  } else if (id == kReqRoomReady) {
    // One-shot: first Ready press in the room launches the match offline.
    if (InterlockedCompareExchange(&g_start_fired, 1, 0) == 0) {
      InterlockedExchange(&g_pending_start, 1);
    }
  }
}

void pn_inject_pump_lobby() {
  if (rmi_inject_disabled())
    return;
  // Create-Room transition: success RES -> RequestState(9) (CGameRoom). Arm the
  // room-populate so the data is in place before CGameRoom's first frame binds.
  if (InterlockedCompareExchange(&g_pending_create_room, 0, 1) == 1) {
    inject_create_room_res();
    InterlockedExchange(&g_pending_populate, 1);
  }
}

void pn_inject_pump_room() {
  if (rmi_inject_disabled())
    return;
  // Runs in CGameRoom::onPreProcess prologue, BEFORE the original binds room
  // data. Populate first (compact map then the UI map the script reads), then
  // honour a pending Ready -> start-match.
  if (InterlockedCompareExchange(&g_pending_populate, 0, 1) == 1) {
    inject_room_enter_res();   // 0x3ED4 compact map
    inject_room_members_res(); // 0x3ED8 UI map (the one that faults when empty)
  }
  if (InterlockedCompareExchange(&g_pending_start, 0, 1) == 1) {
    inject_start_res();
  }
}
