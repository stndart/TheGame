# ProudNet + RMI hook install matrix

**Source of truth:** `[src/thegame/dll_main.cpp](../../src/thegame/dll_main.cpp)` (`install_hooks` → `install_game_stage_hooks` + `install_engine_hooks`). Reimpl bodies live under `[src/ProudNet/](../../src/ProudNet/)`; trampolines under `[src/hooks/](../../src/hooks/)`. RVAs: `[include/ProudNet/Layout.hpp](../../include/ProudNet/Layout.hpp)`, `[docs/proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md)`.


| Column          | Meaning                                            |
| --------------- | -------------------------------------------------- |
| **Installed**   | `HookManager::make_hook` in default `DllMain` path |
| **Stub**        | `HookStub` exists; not installed by default        |
| **Reimpl only** | C++ replacement exists; no hook                    |


---

## Game UI stages (ctl `game_stage`)

All **installed** from `install_game_stage_hooks()`. Emit strings: `[ctl/controller/gamestate/stages.py](../../ctl/controller/gamestate/stages.py)`. Hooks: `[src/hooks/game_state.cpp](../../src/hooks/game_state.cpp)`. Full RVA table: [../proudnet/stages.md](../proudnet/stages.md).


| Installed | RVA        | ctl stage      | Notes                                 |
| --------- | ---------- | -------------- | ------------------------------------- |
| yes       | `0x42A010` | `intro`        |                                       |
| yes       | `0x42B280` | `login`        |                                       |
| yes       | `0x4345B0` | *(none)*       | `server_begin` nav pump only          |
| yes       | `0x4347CC` | `shard_select` | hook symbol `g_target_game_main_menu` |
| yes       | `0x42BD50` | `lobby`        |                                       |
| yes       | `0x4362E0` | `room_list`    |                                       |
| yes       | `0x42F690` | `party_room`   |                                       |
| yes       | `0x439B00` | `room`         |                                       |
| yes       | `0x4F2DB0` | `char_select`  |                                       |
| yes       | `0x4806E0` | `map_loading`  |                                       |
| yes       | `0x47F610` | `in_game`      |                                       |


`connecting_to_server` is emitted from `[src/game/engine/TCPSocket.cpp](../../src/game/engine/TCPSocket.cpp)` on `TCPSocket::Connect`, not a UI hook.

---

## ProudNet transport (installed by default)


| Installed | RVA                                  | Stub / reimpl                                                                 | Mode                         |
| --------- | ------------------------------------ | ----------------------------------------------------------------------------- | ---------------------------- |
| yes       | `0xD6E180`                           | `hooks/socket/pn_upnp_skip.cpp`                                               | full jmp (skip UPnP)         |
| yes       | `0xD55300`                           | `hooks/socket/pn_select_sync.cpp` + `ProudNet/SelectContext.cpp`              | full jmp                     |
| yes       | `0xD71CE0`                           | `hooks/socket/pn_recv_append.cpp` + `ProudNet/RecvAppend.cpp`                 | full jmp                     |
| yes       | `0xD55510`                           | `hooks/socket/pn_recv_complete.cpp` + `ProudNet/FastSocket.cpp`               | full jmp                     |
| yes       | `0xD6EF90` / `0xD6C8E0` / `0xD6C9C0` | `hooks/socket/pn_connection_fsm.cpp` + `ProudNet/ConnectionNode.cpp`          | full jmp                     |
| yes       | `0xD56470`                           | `hooks/socket/proud_fast_socket.cpp` + `ProudNet/FastSocket.cpp`              | full jmp recv                |
| yes       | `0xD567F0`                           | `hooks/socket/send.cpp` (`g_target_send_2`) + `ProudNet/FastSend.cpp`         | full jmp send                |
| yes       | `0xD56220`                           | `hooks/socket/tcpsocket.cpp` + `src/game/engine/TCPSocket.cpp`                | full jmp connect             |
| yes       | `0x14314E0` / `0x014F6DC0`           | `hooks/w_connect_2.cpp`, `w_connect_3.cpp` + `src/game/net/proud_connect.cpp` | full jmp                     |
| yes       | `0xD0C0A0` / `0xD0A340`              | `hooks/net/pn_net_client_factory.cpp`                                         | trace                        |
| yes       | `0xD5DC10` / `0xD5CA30`              | `hooks/net/pn_compress_hook.cpp`                                              | trace passthrough            |
| yes       | `0xD653B0`                           | `hooks/net/pn_process_message_hook.cpp` + `ProudNet/ProcessProudNetLayer.cpp` | full jmp                     |
| yes       | `0xD65940`                           | `hooks/net/pn_drain_recv_hook.cpp` + `ProudNet/DrainReceiveQueue.cpp`         | trace, resume `**0xD65947`** |


---

## RMI capture (installed by default)


| Installed | RVA        | File                            | Mode                                 |
| --------- | ---------- | ------------------------------- | ------------------------------------ |
| yes       | `0xD5C5E0` | `src/RMI/FrameworkSendHook.cpp` | trace (framework heartbeat ids only) |
| yes       | `0x65AEA0` | `src/RMI/GameSendHook.cpp`      | trace (proxy C2S)                    |
| yes       | `0xA0B290` | `src/RMI/GameSendHook.cpp`      | trace (floor C2S)                    |


S2C ids are logged inside `ProcessProudNetLayer` reimpl (`Rmi::LogS2c`), not a separate hook.

---

## Stub only (not in `dll_main.cpp` default set)


| Installed | RVA        | Stub                                                     | Notes                                                              |
| --------- | ---------- | -------------------------------------------------------- | ------------------------------------------------------------------ |
| no        | `0xD569C0` | `hooks/socket/tcpsocket.cpp` (`g_target_TCPSocket_send`) | Fast path uses `send_2` @ `0xD567F0`                               |
| no        | `0xD567F0` | `g_target_fast_wsasend` in `proud_fast_socket.cpp`       | Duplicate RVA with `send_2`; only `send_2` installed               |
| no        | `0xD84BB0` | `hooks/net/pn_tcp_frame_hook.cpp`                        | SEH resume `0xD84BB7`; optional via `TcpLayerMessageExtractor.cpp` |
| no        | `0xD84970` | same                                                     | resume `0xD84977`; never hook `0xD84910`                           |
| no        | `0xD6F7B0` | —                                                        | FSM driver; states 1–3 hooked instead                              |


---

## Reimpl only (no `HookStub`)


| RVA                     | Reimpl                        | Notes                                                                                  |
| ----------------------- | ----------------------------- | -------------------------------------------------------------------------------------- |
| `0x9FC720` / `0x9FCB70` | `ProudNet/GrowableBuffer.cpp` | Called from hooked recv/process paths                                                  |
| `0xD71610`              | `ProudNet/RecvAppend.cpp`     | `ensure_capacity` via append hook                                                      |
| `0xD59300` / `0xD58B30` | —                             | Tail-call GAME from `ProcessProudNetLayer`                                             |
| Per-case handlers       | —                             | Tail-call GAME from reimpl; see [message-dispatch.md](../proudnet/message-dispatch.md) |


---

## SEH / resume rules

1. Do **not** tail-call `game_va(RVA + 5)` on MSVC SEH prologue functions.
2. Set resume from IDA (first safe insn after stolen prefix); drain queue uses `**0xD65947`**.
3. Enable one hook at a time when debugging; full recipe in [../proudnet/implementation.md](../proudnet/implementation.md).

---

## Verify recipe

```powershell
just build
just ctl::copy-dll
just ctl::launch
just ctl::wait-stage shard_select 120
just ctl::copy-logs
```

Inspect `ctl/logs/runs/<run_id>/events.jsonl` and `game_netlogs.txt`.

*Last updated: 2026-06-03 — matches `dll_main.cpp`.*