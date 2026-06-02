# Fake-server RMI inject (removed)

**Status:** removed in v1 (friends server is authoritative). This document archives the harness that lived in `src/RMI/Inject.cpp` before deletion.

**Related:** [client.md](client.md), [autonav.md](autonav.md), [../plans/proudnet-game-rmi.md](../plans/proudnet-game-rmi.md).

---

## Purpose

Offline / dummy-server development could not answer many client C2S game RMIs on the wire: application frames are wrapped in ProudNet transport `0x25`, so the Python replay server often never saw button presses.

The harness **latched** known C2S REQ ids on the send hook and **pumped** matching S2C RES **leaf handlers** in-process on the **main thread**, as if the server had replied.

This was **not** a separate `HookStub` category — it was orchestration on top of existing trace hooks and `IState::onPreProcess` stage hooks.

---

## Architecture

```text
C2S send (worker)     hook_pn_game_rmi_send @ 0x65AEA0
        │
        ▼
NoteC2sSend(id)       sets Interlocked latch only (never calls game state)
        │
        ▼
Main thread           hook_game_lobby / room_list / room onPreProcess
        │
        ▼
PumpLobby / PumpRoom  clears latch → call_res_leaf(leaf_rva, body)
```

**RES leaf convention:** `int __stdcall handler(MsgDelegateArg const* a1)`; body at `*(u32*)(a1+8)`; `result(u16)` at body+2 (`0` = success).

**Direct leaf call** (no full `CReceivedMessage` path):

```c
void *arg[8] = {0};
arg[2] = body;
((int(__stdcall*)(void*))game_va(leaf_rva))(arg);
```

**Threading:** pumping from `pn_drain_recv` was removed — worker-thread inject raced the UI VM. Pumps ran only from stage hooks in [`game_state.cpp`](../../src/hooks/game_state.cpp).

---

## REQ latch → RES inject table

| C2S latch (proxy id) | S2C RES id | Leaf RVA | Pump | Effect |
| --- | --- | --- | --- | --- |
| `0x3F40` lobby-enter notify | `0x3F41` | `0x4BA740` | `PumpLobby` | Lobby-enter ACK; stay on scene 4 |
| `0x3F30` create room | `0x3F30` | `0x437160` | `PumpLobby` | Success → `RequestState(9)` (room) |
| `0x3ED3` room enter / populate arm | `0x3ED4` | `0x4BB560` | `PumpRoom` | Compact member map (32 B records) |
| (same populate latch) | `0x3ED8` | `0x4BB370` | `PumpRoom` | UI member map (168 B records, slot 0) |
| `0x3F2B` Ready (one-shot) | `0x3F3D` | `0x43D9B0` | `PumpRoom` | Start match → scene 11 if play ctx set |
| `0x3F45` leave room | `0x3F45` | `0x43D020` | `PumpRoom` | Leave-room RES |

**Nav-only direct calls:** `InjectLobbyEnterRes()`, `InjectLeaveRoomRes()` (no C2S latch).

**Floor-band note:** latches watched **proxy** ids from `sub_65AEA0` only; floor sends via `sub_A0B290` were logged but not latched.

---

## Source files (deleted)

| File | Role |
| --- | --- |
| `src/RMI/Inject.cpp` | Latches, pumps, synthetic bodies |
| `include/RMI/Inject.hpp` | `NoteC2sSend`, `PumpLobby`, `PumpRoom`, inject helpers |
| `src/RMI/GameSendHook.cpp` | Called `NoteC2sSend` from proxy log hook |
| `src/hooks/game_state.cpp` | `PumpLobby` on lobby/room_list; `PumpRoom` on room |
| `src/RMI/Nav.cpp` | Called `InjectLobbyEnterRes` in offline nav |

**Config:** `DISABLE_RMI_INJECT` (CMake default ON) gated runtime behavior when inject was still compiled in.

---

## Why removed for v1

| Problem | Detail |
| --- | --- |
| **Doubles real S2C** | Friends server answers the same ids; inject + wire RES race and desync UI |
| **Wrong thread history** | Early worker-thread pump caused VM faults; main-thread pump fixed offline but not online |
| **Hides server bugs** | Client appeared healthy while server wire was wrong |
| **Autonav coupling** | `nav_goto_lobby` called `InjectLobbyEnterRes` instead of waiting for server |

**Replacement:** trust server S2C; use `[rmi]` trace hooks (C2S + S2C) and ctl `nav_goto_lobby` (C2S enter only).

---

## Historical verification

| Run / doc | Note |
| --- | --- |
| Offline nav matrix runs **228–232** | Inject on; full lobby→room path |
| Run **207** | Lobby → `shard_choice` bounce without inject — fragile wire-only |
| [autonav.md](autonav.md) legacy sections | Env chaining + inject cooperation |

---

## Log lines (no longer emitted)

Legacy prefix `inject:` (e.g. `inject: lobby-enter RES 0x3F41`). Grep for `[inject]` or `inject:` should find nothing in new builds.
