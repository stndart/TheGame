# Long: CGameRoom onPreProcess UI-VM crash + injection thread

**Date:** 2026-05-29

## Symptom

- Run 180: create-room injection (`0x3F30` -> `sub_437160` -> `RequestState(9)`) transitioned
  `room_list` -> `room` and was stable enough to toggle Ready/team.
- Run 181: same flow hung/crashed immediately after the create dialog. Fault `0xC0000005` at
  `0x01307B36` in `sub_1307B30`, instruction `mov edi,[ecx+40h]` (i.e. `this->[0x40]`).

## Crash anatomy (from subagent 94fa509d RE)

- `sub_1307B30` = UI-variant VM "store-to-member" helper. Only callers are VM opcode handlers
  `sub_130AA80` (`0x130AB06`) and `sub_130AB20` (`0x130ABA8`):
  `sub_1307B30(*(_DWORD **)(v9 + 20), &a2, a2 - 1, v7, v9)` where `v9` is a popped VM stack slot.
  Inside, `this = stackSlot.field8->field20`; the fault is `*(this+0x40)` with a garbage `this`.
- The VM is driven by `CGameRoom::onPreProcess` (`0x439B00`, vtable slot 15). On entry it marshals
  room/member fields into UI variants via `sub_442F90` (quick-character / quick-select panel),
  `sub_4419F0` + siblings (custom-match path, gated `dword_1C1A87C >= 1`), and many
  `sub_63E9B0("Set…", variant, flag)` calls (`SetDiveRoomMessage`, `SetAutoMatchLobby`,
  `SetQuickCharacterInfo`, `SetQuickSelect`, …). Variants carry the `-1,-1` null-handle sentinel
  (`v23[0]=v23[1]=-1`). With an empty room, one binding resolves its target against a stale/`-1`
  pointer and the VM faults at `+0x40`.

## Object allocation (NOT a missing singleton)

`sub_41A920` (called once from app-init `sub_61FC70` @ `0x62051B`) allocates at boot:

| global | via | role |
|---|---|---|
| `dword_1C10C94` | `sub_439900` (CGameRoom ctor, slot `this+60`) | CGameRoom IState singleton |
| `dword_1C10C98` | `sub_4445F0` (`this+65`) | room/lobby data manager |
| `dword_1C10D3C` | `sub_4F6120` (`this+67`) | room-state record (~40 readers) |
| `dword_1C10D44` | `sub_4FF690`/`sub_4FF820` | member/notify list (append `sub_507E70`) |

So after a bare `0x3F30` transition these are non-null but empty/zeroed. The crash is empty
*contents* (member map `dword_1C2EA08`, member objects `off_1C28684`, state fields in
`dword_1C10D3C`, local-player `dword_1BE90AC`), not a missing allocation.

## RES handlers (body offsets from `body = *(u32*)(a1+8)`)

- `0x3ED4` -> `sub_4BB560` -> `sub_962020(body)` (fill map `dword_1C2EA08`) + `sub_8DA900`
  (copy each 32B record into member objects via `sub_8FAB80(&off_1C28684)`).
  `+0` u16 (unused), `+2` int count, `+6` count×32B records. All-zero (count=0) -> loops skipped ->
  safe but empty. **Coherent count=1 local-player record is the real fix — layout being decoded.**
- `0x3F11` -> `sub_4BB7E0`: member-name record (`+2` id, `+6` id2); enqueues type-17 via
  `sub_507E70(dword_1C10D44)`. Zero body enqueues phantom id 0 (guarded, no crash).
- `0x3F47` -> `sub_4BAEB0`: player record (`+2` id); unknown id -> guarded no-op.
- `0x3F03` -> `sub_4BB8F0`: lobby/room counter (`+2`). **RISKY** — `sub_5EBD20`/`sub_5EBC00` key
  validation can hit `invalid_parameter_noinfo()` on a zero/mismatched key. Do not inject blind.
- `0x3F81` -> `sub_4BB450`: room-state refresh (`+2` state). Guarded; in state 9 it calls
  `sub_442F90()` and returns. Does NOT create or transition. Re-runs the same UI path that crashes
  on an empty room, so deferred until the room is populated.

## Thread evidence

- `CNetClientImpl::OnMessageReceived` (`sub_D65940`, our `drain_receive_queue` hook) drains the recv
  queue and dispatches per message — observed on tids 32880/13432, distinct from the main thread
  28308 (CGameXxx ctor / app-init). So dispatch + RES leaves run on the ProudNet worker thread.
- `Diagnostics::emit_game_state` dedups via `g_last_game_phase`, which is why each stage prints once
  even though `IState::onPreProcess` (the hooked `0x439B00` etc.) fires every frame on the main
  thread. That dedup hid the per-frame cadence and made the onPreProcess hooks look like onEnter.

Conclusion: pumping injection from the drain hook ran `RequestState`/UI/VM work on the worker thread,
racing the main-thread frame loop -> intermittent `0x1307B30` fault.

## Fix applied

Latch on send (any thread), pump on the main thread from the state `onPreProcess` prologue hooks:
- `room_list` onPreProcess -> `pn_inject_pump_lobby()` -> create-room transition RES + arm populate.
- `CGameRoom` onPreProcess prologue -> `pn_inject_pump_room()` -> populate (count=0 WIP) then
  pending Ready->start. Runs before the jump to the original onPreProcess, so populated data would be
  in place before the first bind.
Removed the worker-thread `pn_inject_pump()` call from `drain_receive_queue`.

## Recommended injection order (real-server parity, from subagent)

After `0x3F30` create OK (-> scene 9): inject `0x3ED4` with >=1 valid member (local player id +
non-null name handle) FIRST, then optionally `0x3F81` with a real state value. `0x3F11`/`0x3F47` per
extra member with matching ids (guarded). Never inject `0x3F03` with a zero body.

## Open / next

- Decode the 32B `0x3ED4` member record (offsets for id, name handle, team, slot, character, ready)
  and the source of the local player id (likely `dword_1BE90AC` + offset, account_id from connect
  RES = 1). Replace the count=0 placeholder with a coherent count=1 local-player record.
- Confirm whether `0x3F81` is needed once a coherent member exists; if so, inject after populate.
- Re-run lobby -> Custom Match -> Create Room -> Ready and watch for `map_loading`/`in_game`.
- If it still faults: runtime-break `0x1307B36`, read `ecx` / `stackSlot.field8->field20`, map the
  bound object back to whichever of `0x3ED4`/`0x3F11`/`0x3F47` owns that field.
