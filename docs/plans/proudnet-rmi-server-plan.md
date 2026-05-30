# Game RMI layer & server-emulation plan (path to "client loads the map")

**Status:** living doc, 2026-05-29. Application-layer companion to transport docs ([../proudnet/protocol.md](../proudnet/protocol.md), [../proudnet/message-dispatch.md](../proudnet/message-dispatch.md)). **Full RMI reference (dispatch tables, body layouts, injection):** [proudnet-game-rmi.md](proudnet-game-rmi.md).

## Goal

Client **loads a map** (live match level) from the offline path that already reaches lobby.

## Strategy

1. RE game RMI semantics; replace opaque FA-EMU replay **one S2C at a time** ([proudnet-game-rmi.md](proudnet-game-rmi.md) §0–10).
2. **DLL** = C2S capture + optional in-process RES injection; **Python server** = wire S2C builders (`server/server/proud_rmi.py`, `game_transport.py`).
3. C2S app RMIs are inside transport `0x25` (opaque on socket); S2C game RMIs are plaintext inner `0x02`+id+body.

## Verify loop

```
just ensure-serve → just ctl::copy-dll → just ctl::launch-offline → just ctl::wait-stage <stage>
→ just ctl::copy-logs → ctl/logs/runs/<run>/{events.jsonl,game_netlogs.txt}
```
Default **debug** DLL has inject compiled out (wire/server S2C). Create-room wire **PASS:** run **`187_eafd5dd0`** (242 B burst `0x3F30`+`0x3ED4`+`0x3ED8`, no `inject:` line; `game_state: room`). See [journals/2026-05-29-05-wire-s2c-rmi-builders.md](../journals/2026-05-29-05-wire-s2c-rmi-builders.md).

## Game-state machine → ctl stages

Hooks [`src/hooks/game_state.cpp`](../src/hooks/game_state.cpp). Verified through run 181.

| Stage | Class | onPreProcess RVA |
|-------|-------|------------------|
| `intro` | CGameIntro | 0x42A010 |
| `login` | CGameLogin | 0x42B280 |
| `shard_choice` / `server_ready` | CGameServer | 0x4345B0 / 0x4347CC |
| `lobby` | CGameLobby | 0x42BD50 |
| `room_list` | CGameRoomList | 0x4362E0 |
| `party_room` | CGameMatchPartyRoom | 0x42F690 |
| `room` | CGameRoom | 0x439B00 |
| `char_select` | CGameCharacterSelect | 0x4F2DB0 |
| `map_loading` | CBasePlayLoading | 0x4806E0 |
| `in_game` | CGamePlay | 0x47F610 |

## S2C wire construction matrix

**Columns:** RMI id (or transport op) | name | server builder | test | notes / C2S REQ (run **179**, proxy/floor)

**Test:** PASS = verified live or unit framing test; WIP = builder exists, not E2E; NO = no builder; BLOB = verbatim capture only.

### Transport opcodes (not game RMI ids)

| ID | Name | Server builder | Test |
|----|------|----------------|------|
| `0x04` | KEY_BLOB (ENTRY) | `build_key_blob_frame` | PASS |
| `0x06` | ACK (ENTRY) | `entry_transport` | PASS |
| `0x0A` | REDIRECT (ENTRY) | `entry_transport` | PASS |
| `0x1C` | KEEPALIVE | `build_keepalive_frame` | PASS |
| `0x1D` | PONG | `build_pong_frame` | WIP |
| `0x02` | ENTRY server-info ×2 | `build_entry_serverinfo_pair` | BLOB |
| `0x25` | GAME session wrapper | (carries C2S; S2C replies are inner `0x02`) | - |

### Lobby replay (`lobby_replay.json` / `lobby_postinit.json`)

**Runtime default:** full JSON bursts (`LOBBY_USE_CONSTRUCTED_FRAMES=false`). **Unit tests:** capture-exact builders in `proud_rmi.py` (19 RMIs in `test_wire_lobby_rmis.py`). **Experimental runtime:** `LOBBY_USE_CONSTRUCTED_FRAMES=true` (burst 0 byte-identical; burst 1 shorter; postinit from builders).

| ID | Name | Server builder | Test |
|----|------|----------------|------|
| *(burst 0)* | 10× inner `0x02` + op `0x27` + 5904 B tail | `build_lobby_burst0_*` + tail from JSON | PASS (unit); BLOB (runtime default) |
| `0x3EDC`…`0x3ECA` | burst-0 framed RMIs | `build_lobby_burst0_*_frame` | PASS (unit) |
| `0x3E8E` | NetConnectRES (in unframed tail) | `build_net_connect_res_frame` (64 B stub) | WIP |
| `0x3E99` | notify 15060 | `build_notify_15060_frame` | PASS (unit) |
| *(burst 1)* | connect burst | `build_net_user_connect_res_frame` + tail OR full JSON | PASS (644 B unit); BLOB (runtime) |
| `0x3F3E` | NetUserConnectRES (644 B) | `build_net_user_connect_res_frame` | PASS (unit) |
| `0x3F18`/`0x3EFC` | burst-1 prefix | - | BLOB |
| *(burst 2)* | post-connect | `LOBBY_REPLAY_BURSTS[2]` | BLOB |
| `0x3F60`/`0x3EA9`/`0x3F4E`/`0x3EC6` | burst-2 RMIs | - | NO |
| `postinit` | `0x3EA6`×5, `0x3F8D`, `0x3ED2`, `0x3F8C` | `build_lobby_postinit_*` | PASS (unit); BLOB (runtime default) |
| `0xD23E` | periodic tick | `PN_D23E_TICK` from postinit | BLOB |

Inventory: `uv run python -m server.tools.scan_lobby_replay` from `server/`.

### Generated S2C (`proud_rmi.py` → `game_transport.py`)

| ID | Name | Server builder | Test | C2S REQ (179) |
|----|------|----------------|------|----------------|
| `0x3F3E` | NetUserConnectRES | `build_net_user_connect_res_frame` | PASS | connect path |
| `0x3E8E` | NetConnectRES | `build_net_connect_res_frame` | WIP | - |
| `0x3E99` | notify / 15060 | `build_notify_15060_frame` | WIP | `0x3E99`/`0x3AD4` |
| `0x3F30` | create-room RES | `build_create_room_res_frame` / `build_create_room_res_burst` | **PASS** (187) | `0x3F30`/`0x3AA0` len 98 |
| `0x3ED4` | room-enter (compact) | `build_room_enter_res_frame` | PASS (187 chain) | `0x3ED3`/`0x3AC0` len 6 |
| `0x3ED8` | room members (UI) | `build_room_members_ui_res_frame` | PASS (187 chain) | (after create) |
| `0x3F81` | room state | `build_room_state_res_frame` | WIP | - |
| `0x3F11` | member name | `build_room_member_name_res_frame` | WIP | - |
| `0x3F3D` | start-match RES | `build_start_match_res_frame` | WIP | `0x3F3D`/`0x3AA9` len 2 |
| `0x3F45` | leave-room RES | `build_leave_room_res_frame` | WIP | `0x3F45`/`0x3AA3` len 6 |
| `0x3F2F` | room-list RES | - | NO | `0x3F2F`/`0x3A9F` len 2 |
| `0x3F41` | lobby-enter RES | - | NO | `0x3F40`/`0x3ACE` len 2 |
| `0x3EE4` | Quick Dive | - | NO | `0x3EE4`/`0x3AAF` len 3 |
| `0x3F2B`→`?` | Ready RES | - | NO | `0x3F2B`/`0x3AA8` len 3 |
| `0x3F03`/`0x3F47`/… | room sync (account band) | - | NO | various |

Triggers: `CREATE_ROOM_RES_ON_25_BODY_LEN=115`, burst mode - see `server/.env` and [proudnet-game-rmi.md](proudnet-game-rmi.md) §10.

### Inject-only (`src/RMI/Inject.cpp`, compile-time opt-in)

| ID | Name | Mechanism | Test | C2S latch |
|----|------|-----------|------|-----------|
| `0x3F30` | create → scene 9 | `inject_create_room_res` @ `0x437160` | PASS (180) | `0x3F30` |
| `0x3ED4` | compact member map | `inject_room_enter_res` @ `0x4BB560` | WIP | after create pump |
| `0x3ED8` | UI member map | `inject_room_members_res` @ `0x4BB370` | WIP | after create pump |
| `0x3F3D` | start → scene 11 | `inject_start_res` @ `0x43D9B0` | WIP | `0x3F2B` Ready |

Pump on **main thread** from `room_list` / `CGameRoom` onPreProcess ([journal](../journals/2026-05-29-03-cgameroom-uivm-crash-thread.md)).

## C2S REQ map (run 179, condensed)

Full timeline: [journals/long/2026-05-29-02-c2s-rmi-per-action-ids-run179.md](../journals/long/2026-05-29-02-c2s-rmi-per-action-ids-run179.md). Canonical log: `events.jsonl` (not truncated `game_logs.txt`).

| Action | Proxy | Floor | len |
|--------|-------|-------|-----|
| server select | `0x3F0C` | `0x3AD1` | 2 |
| lobby enter | `0x3F40` | `0x3ACE` | 2 |
| global chat | - | `0x3AD2` | 36 |
| Quick Dive ×2 | `0x3EE4` | `0x3AAF` | 3 |
| room list | `0x3F2F` | `0x3A9F` | 2 |
| **create room** | **`0x3F30`** | **`0x3AA0`** | **98** |
| team/loadout | - | `0x3AC6` | 10246 |
| ready | `0x3F2B` | `0x3AA8` | 3 |
| start | `0x3F3D` | `0x3AA9` | 2 |
| leave | `0x3F45` | `0x3AA3` | 6 |

REQ/RES pairing and body layouts: [proudnet-game-rmi.md](proudnet-game-rmi.md) §7–9 (not REQ=RES−1).

## Current frontier

- **Done:** wire create-room burst → `room` (187); inject create → `room` (180).
- **Next:** coherent `0x3ED8` slot-0 record (wire + inject); wire `0x3F3D` start; disambiguate `0x25` len=19 (enter/ready/leave).
- **Later:** Quick Dive (`0x3EE4`), room-list RES, map-load game-server connect.

## Superseded (2026-05-29)

- **§ "REQ id = RES id − 1"** - wrong for this build; REQ and RES often share the same id (e.g. `0x3F30`). Use registrar tables in [proudnet-game-rmi.md](proudnet-game-rmi.md) §7.
- **§ "Current frontier / run 176"** - create-room had no server answer; superseded by run 179 REQ capture, 180 inject, **187 wire PASS**.

## Files

| Area | Path |
|------|------|
| Stages / inject pump | [`src/hooks/game_state.cpp`](../src/hooks/game_state.cpp), [`src/RMI/Inject.cpp`](../src/RMI/Inject.cpp) |
| C2S hooks | [`src/RMI/GameSendHook.cpp`](../src/RMI/GameSendHook.cpp) |
| Wire builders | [`server/server/proud_rmi.py`](../server/server/proud_rmi.py), [`game_transport.py`](../server/server/game_transport.py) |
| Offline tests | `server/server/test_wire_create_room.py`, `test_wire_start_match.py`, `test_wire_leave_room.py` |
