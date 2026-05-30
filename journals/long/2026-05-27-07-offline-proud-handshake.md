# Long: offline ProudNet handshake to main menu (2026-05-27)

## Goal

Reach `ctl` game_state stages `shard_choice` and `main_menu` with `just ctl::launch-offline` against the local Python ProudNet emulator (`just ensure-serve`), not FA-EMU production hosts.

## End-to-end flow

```text
127.0.0.1:7000  ENTRY transport (entry_transport.EntrySession)
  → redirect token / host in 0x0a frame → 127.0.0.1:27380
127.0.0.1:27380 GAME transport (game_transport.GameSession)
  → repeat 0x2f..0x0a → client 0x25 → lobby_replay.json burst(s)
  → postinit keepalives / d23e tick from lobby_postinit.json
Client RMI dispatch → onNetUserConnectRES / lobby UI → shard_choice → main_menu hooks
```

Optional probe leg `:20009` (raw `0x2f` echo) matches FA-EMU `tcpServer20009`; not on critical path for menu.

## Root cause 1 - server answered `0x2f` with RMI

Early `server/` prototype replied on `:7000` with a synthetic **NetUserConnectRES** (Proud RMI `0x13` stream) immediately after the client hello. The client PN worker expects the **FA-EMU transport opcode chain** first (`master_server.js` / `entry_transport.py` header comments).

Symptom: FSM stuck after first recv; no redirect to `:27380`; no `shard_choice`.

**Fix:** `server/server/entry_transport.py` - `C2S 0x2f → S2C 0x04` key blob, then `0x05`/`0x06`, `0x07`/`0x0a` redirect, `0x25` paired `0x02` server-info frames. RMI only after GAME leg session (`config.SEND_RMI_AFTER_ENTRY=false` in `server/server/config.py`).

## Root cause 2 - `pn_select_sync` overwrote GAME fd with ENTRY fd

`sub_D55300` (`src/hooks/socket/pn_select_sync.cpp`) passes `CFastSocket` ctx as `optval`. Original hook always copied the tracked **ENTRY** socket into handle slot @ offset 300.

When the worker advanced to the GAME leg, select still pointed at `:7000`; wire showed transport `0x2f` on ENTRY again instead of GAME handshake.

**Fix:** `src/game/net/socket_trace.cpp` - buckets `g_sockets_by_port` for `:7000`, `:27380`, `:20009`; `sync_fast_socket_handle` returns early when slot peer is GAME or PROBE (comment @ line 114).

## Root cause 3 - `:27380` hit production (remap bypass)

`TCPSocket::Connect` (`src/game/engine/TCPSocket.cpp`) and `ws32` `connect` IAT covered `:7000`, but ProudNet also uses:

- `w_connect_3` @ `0x1431A30` - partial connect with `sockaddr_in` already in object (`src/hooks/w_connect_3.cpp`)
- Other static `connect` imports not passing through `TCPSocket`

Unremapped `216.131.86.188:27380` or production redirect host reached live FA-EMU.

**Fix:** `ServerOverride::remap_sockaddr_in` (`src/game/server_override.cpp`, `include/game/server_override.hpp`) centralizes host/port remap; `connect_syshandle` in `src/hooks/system/ws32.cpp`; `HookManager::make_hook(g_target_w_connect_3)` in `src/main.cpp`. Logs: `connect:27380` / `w_connect_3:27380` in `game_netlogs.txt`.

## Root cause 4 - `ensure-serve` only checked `:7000`

Stale partial server (ENTRY up, GAME down) looked “healthy” when only `config.PORT` (7000) was probed. Client completed ENTRY then hung on GAME connect.

**Fix:** `server/server/process_lock.py` - `required_listen_ports()` → `[7000, 27380, 20009]`; `server/server/ensure_serve.py` waits on `all_listen_ports_open()` and errors on partial listener with `taskkill` cleanup.

## Server files (three-port lifecycle)

| File | Role |
|------|------|
| `server/server/config.py` | `PORT`/`GAME_PORT`/`PROBE_PORT`, `REDIRECT_HOST`, RMI flags |
| `server/server/serve.py` | Accept loops, `EntrySession` / `GameSession` / probe handler, `WireLogger` → `ctl/logs/server/wire_*.log` |
| `server/server/entry_transport.py` | ENTRY FSM + captured server-info prefix/postfix |
| `server/server/game_transport.py` | GAME FSM, `LOBBY_REPLAY_BURSTS`, postinit scheduler |
| `server/server/data/lobby_replay.json` | Hex frames for first `0x25` on `:27380` |
| `server/server/data/lobby_postinit.json` | Post-login keepalive / `d23e` tick frames |
| `server/server/proud_frame.py`, `proud_rmi.py` | Framing + RMI builders (fallback if replay missing) |

## Client hooks / trace

| Path | Notes |
|------|-------|
| `include/game/net/socket_trace.hpp` | Port constants aligned with `ServerOverride` |
| `src/game/net/socket_trace.cpp` | `track_connect`, per-port sets, guarded sync |
| `src/hooks/socket/pn_select_sync.cpp` | `hook_pn_select` @ `0xD55300` |
| `src/hooks/socket/pn_recv_append.cpp` | Recv append trace (supporting) |
| `src/hooks/socket/send.cpp` | `w_wsasend_1` logging |
| `include/game/net/proud_frame.hpp` | Client-side frame parse helpers |

Prior journals: `2026-05-27-01-connect-stage-protocol.md` (PN FSM IDA map), `2026-05-27-02-offline-ip-and-stage-hooks.md` (IP remap), `2026-05-27-03-proud-framing-and-recv-trace.md` (RMI layout).

## Verified session `018_5cb743bb`

Commands (from repo root):

```text
just ensure-serve
just ctl::copy-dll
just ctl::launch-offline
just ctl::wait-stage shard_choice --timeout 120
just ctl::wait-stage main_menu --timeout 120
just ctl::copy-logs
```

Evidence:

- `ctl/logs/runs/018_5cb743bb/events.jsonl` - daemon `[stage]` / `game_state` transitions through `shard_choice`, `main_menu`
- `ctl/logs/runs/018_5cb743bb/game_netlogs.txt` - `remap` / `connect:27380` / `w_connect_3:27380` → `127.0.0.1`
- `ctl/logs/server/wire_<timestamp>.log` - `[GAME host:port] 0x2f → 0x04` on `:27380`, lobby replay TX after `0x25`

Contrast run `009_96369131` (pre-fix): massive `game_netlogs` from misrouted `0x2f` on `:7000`; no `main_menu`.

## Open items

- Lobby replay is capture-derived; new client builds may need refreshed `lobby_replay.json`.
- `w_connect_2` @ `0x14314E0` still log-only; watch for new bypass paths in IDA.
- Gameplay (grenades, match) still needs human check - network path to menu is unblocked.
