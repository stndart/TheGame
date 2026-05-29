# Game RMI layer & server-emulation plan (path to "client loads the map")

**Status:** living doc, started 2026-05-29. This is the **application-layer (game RMI)** companion to the transport docs ([proudnet-offline-protocol.md](proudnet-offline-protocol.md), [proudnet-message-dispatch-map.md](proudnet-message-dispatch-map.md), [proudnet-ida-symbol-map.md](proudnet-ida-symbol-map.md)). It covers what the **server must send** to drive the client forward, and how we RE + verify it.

## Goal

Get the client to **load a map** (enter a live match level), starting from the offline path that already reaches the lobby.

## Strategy (confirmed)

1. **RE the game RMI protocol**, do not extend the opaque FA-EMU replay. The wall is **not crypto** — game RMI is plaintext on this build's offline path (`NetUserConnectRES 0x3F3E` is built as a plain `0x02`+id+body frame and works). The unknown is the **request/response semantics** of the lobby→room→match→map flow.
2. **The DLL (`TheGame.dll`) is the RE microscope + verification harness:** decode/log C2S RMI requests, instrument S2C handlers to learn required response layouts, and emit ctl **stages** so map-load is observable in logs.
3. **Generate S2C responses** (plaintext RMI frames) in the server, replacing opaque replay **one message at a time**. Fallback for any response that needs decrypted C2S content: **in-DLL RMI injection** (bypass the wire).
4. **Capture happens at the RMI layer in-process, NOT on the wire.** C2S requests are wrapped inside transport opcode `0x25` (reliable/compressed) and are unreadable on the socket; S2C replay is plaintext `0x02`.

## Verified iterate loop

```
edit C++ / server  →  just build-debug  →  just ctl::copy-dll
just ctl::launch-offline  →  (human navigates if past lobby)  →  just ctl::wait-stage <stage> <timeout>
read ctl/logs/runs/<run>/events.jsonl  +  G:\Games\FA\FA-EMU\Shipping\proudnet_tcp.txt (TX/RX frames)
```
- `just ctl::kill-all` if the daemon reports a stale "Game is already running".
- Benign in events.jsonl: `0x406D1388` (SetThreadName). Faults to watch: `0xC0000005`, `0xC0000096`, `0xE06D7363`.

## Game-state machine → ctl stages (verified run 176, 2026-05-29)

States are `IState`-derived `CGameXxx` singletons; `onPreProcess` (vtable slot 15) = onEnter. Hooks in [`src/hooks/game_state.cpp`](../src/hooks/game_state.cpp), prologue `55 8B EC 83 E4 F8`, resume = RVA+6.

| Stage | Class | onPreProcess RVA | Notes |
|-------|-------|------------------|-------|
| `intro` | CGameIntro | 0x42A010 | |
| `login` | CGameLogin | 0x42B280 | |
| `shard_choice` | CGameServer (begin) | 0x4345B0 | server/shard picker |
| `server_ready` | CGameServer (end) | 0x4347CC | **was the deceptive `main_menu`**; not a menu, just onEnter end. mov ecx,imm32 patch, resume 0x4347D1 |
| `lobby` | CGameLobby | 0x42BD50 | **= the "main menu"**: global chat + Quick Match / Custom Match buttons |
| `room_list` | CGameRoomList | 0x4362E0 | Custom Match room list + Create |
| `party_room` | CGameMatchPartyRoom | 0x42F690 | |
| `room` | CGameRoom | 0x439B00 | waiting room (triggers automatch) |
| `char_select` | CGameCharacterSelect | 0x4F2DB0 | |
| `map_loading` | CBasePlayLoading | 0x4806E0 | **level load begins**; onUpdate FSM 0x4801F0; map id = `dword_1C1A980`; begin-load call `sub_6D9C70`; entry to play gated on network connect to game server |
| `in_game` | CGamePlay | 0x47F610 | **end goal** (scene id 11/21) |

State manager: `CStateMachine` singleton `dword_1C15640`; active state @ `+128`; `dword_1C15644` = current scene id, `dword_1C1564C` = requested next scene.

## Game RMI dispatch map (S2C receive handlers)

Registrars: **CAccountPacket::Register `sub_4B8630`** (binder `sub_556CB0`, 55 ids) and **CCommonPacket::Register `sub_421070`** (binder `sub_554930`, 43 ids). Everything registered is **S2C** (what the server must SEND). C2S REQ halves are separate send-proxies (not in these tables); **REQ id = RES id − 1**.

Key anchors (full table in agent transcript / to be expanded here as used):
- **Connect:** `0x3F3E` onNetUserConnectRES (`0x4BA070`, ~384B body) — REQ `0x3F3D`.
- **Room/match cluster:** `0x3F81` (room/match state, casts CGameMatchPartyRoom/CGameRoom @ `0x4BB450`), `0x3F03` (room/lobby counter `0x4BB8F0`), `0x3F11` (room member/name list `0x4BB7E0`), `0x3F47` (player/room record).
- **In-match band:** `0x36Bx`–`0x371x` (entity/position/playlogic sync, CCommonPacket).
- Two numbering bands: `0x36xx` (in-match gameplay), `0x3Exx`/`0x3Fxx` (account→lobby→room→shop).

## Current frontier / open gaps

- **Create-Room and Quick-Match produce no transition** (run 176): the client sends a `0x25`-wrapped REQ, the replay server has no answer, so the client stays in `room_list`/`lobby`. These are the first two requests to decode + answer.
- **TODO (in progress):** locate the generic outbound RMI send chokepoint to log C2S `rmi_id`+body in-process; identify Create-Room / Quick-Match REQ ids; then RE their RES bodies and generate them.

## Files

- Stages: [`src/hooks/game_state.cpp`](../src/hooks/game_state.cpp), [`stubs/target_hooks.h`](../stubs/target_hooks.h), registered in [`src/main.cpp`](../src/main.cpp).
- Wire trace/decoder: [`src/game/net/pn_tcp_trace.cpp`](../src/game/net/pn_tcp_trace.cpp) (`PnTcpTrace::log_chunk`), parsers in `include/game/net/proud_frame_parse.hpp` + `proud_message.hpp`.
- Dispatch reimpl: [`src/game/net/pn_process_message.cpp`](../src/game/net/pn_process_message.cpp).
- Server: `server/server/game_transport.py`, `proud_rmi.py`.
