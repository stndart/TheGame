# ProudNet client hooks - current status

**Last verified:** 2026-05-28 - ctl run **`175_4057145c`**, offline `shard_choice` in ~55s.

Authoritative install list: [`src/main.cpp`](../../src/main.cpp) (`HookManager::make_hook`). RVAs and resume addresses: [`include/ProudNet/Layout.hpp`](../../include/ProudNet/Layout.hpp).

---

## Stable configuration (enabled)

| Hook | RVA | Mode | Implementation |
|------|-----|------|----------------|
| `TCPSocket_connect` | `0xD56220` | **full jmp** | `src/game/engine/TCPSocket.cpp` |
| `send_2` / fast WSASend | `0xD567F0` | **full jmp** | `pn_fast_socket.cpp` |
| `fast_wsarecv` | `0xD56470` | **full jmp** | `pn_fast_socket.cpp` |
| `pn_upnp` | `0xD6E180` | **full jmp** (skip) | `pn_upnp.cpp` |
| `pn_recv_append` | `0xD71CE0` | **full jmp** | `pn_recv_append.cpp` |
| `pn_select` | `0xD55300` | **full jmp** | `pn_select.cpp` |
| `pn_recv_complete` | `0xD55510` | **full jmp** | `pn_fast_socket.cpp` |
| `pn_fsm_state1` | `0xD6EF90` | **full jmp** | `pn_connection.cpp` |
| `pn_fsm_state2` | `0xD6C8E0` | **full jmp** | `pn_connection.cpp` |
| `pn_fsm_state3` | `0xD6C9C0` | **full jmp** | `pn_connection.cpp` |
| `pn_net_client_factory` | `0xD0C0A0` | **trace** tail-jump | `pn_net_client_factory.cpp` → resume `0xD0C0A7` |
| `pn_net_client_ctor` | `0xD0A340` | **trace** | `restore_hook` + original (SEH entry) |
| `pn_process_proudnet_layer` | `0xD653B0` | **full jmp** | `pn_process_message.cpp` |
| `pn_process_compressed` | `0xD5DC10` | **trace** passthrough | `restore_hook` → full entry RVA |
| `pn_process_encrypted` | `0xD5CA30` | **trace** passthrough | `restore_hook` → full entry RVA |
| `pn_drain_receive_queue` | `0xD65940` | **trace** tail-jump | SEH resume `0xD65947`; worker = `*(client+0x5C)` |
| `w_connect_2` / `w_connect_3` | `0x14314E0` / `0x14F6DC0` | **full jmp** | `proud_connect.cpp` |

IAT traces (`connect`, `send`, `WSASend`, …): `src/hooks/system/ws32.cpp`.

---

## Disabled (WIP - do not enable without re-verify)

| Hook | RVA | Blocker |
|------|-----|---------|
| `pn_tcp_frame_recv` | `0xD84BB0` | SEH prologue; enabling caused early disconnect (run `152`). Code ready: `restore_hook` + resume **`0xD84BB7`** in `pn_tcp_frame_hook.cpp`. |
| `pn_tcp_frame_send` | `0xD84970` | Same SEH pattern; resume **`0xD84977`**. Never hook in-function **`0xD84910`** (E8 call site). |
| `TCPSocket_send` | `0xD569C0` | Not required for offline path. |
| `pn_drain_*` (old ABI) | - | Superseded by fixed `0xD65940` hook above. |

---

## Hooking rules (learned 2026-05-28)

1. **Never tail-call `game_va(RVA + 5)`** on functions with an MSVC **SEH prologue** (`push -1; push handler; mov eax, fs:[0]; …`). The 5-byte `E9` patch splits the second `push imm32` → crash (`0xC0000096` @ entry+`0xF`).
2. **Use IDA** (IDA Pro MCP when available, or IDA UI) to read the first instructions and set **`k*Resume`** = first safe address **after** the stolen prefix (typically **+7** for the 7-byte SEH prefix). Record in `pn_layout.hpp` / hook file comments.
3. **Do not** dump prologue bytes from `GAME.exe` with ad-hoc Python scripts; use **IDA MCP** (`get_bytes`, decompile, xref) or existing journals.
4. **Full jmp** reimpls may still call GAME via `game_va(full_RVA)` + `restore_hook` for sub-handlers (compress, `Message_Read`, per-case handlers) until those are reimplemented.
5. **`HookManager::make_hook` / `restore_hook`** are guarded by a critical section (patch races during multi-threaded connect).
6. Enable new hooks **one at a time**; verify: `just ctl::copy-dll` → `just ctl::launch-offline` → `just ctl::wait-stage shard_choice 240`.

---

## ProcessMessage layer

- **Reimpl:** `process_message_proudnet_layer` @ `0xD653B0` - reads type via `Message_Read`, dispatches ~50 cases; handlers still tail-call GAME RVAs from [`pn_layout.hpp`](../../include/game/net/pn_layout.hpp).
- **Cases 37–39:** unwrap via `restore_hook` + `kProcessCompressed` / `kProcessEncrypted` (full entry, not `+5`), then recurse layer.
- **Enum / table:** [`MessageType.hpp`](../../include/ProudNet/MessageType.hpp), [../proudnet/message-dispatch.md](../proudnet/message-dispatch.md).

Toggle trace vs full reimpl for the layer (debug): env `THEGAME_PN_PROCESS_FULL_REIMPL` in `pn_process_message_hook.cpp` (default full reimpl).

---

## Remaining work (full jump path)

Construction → sockets goal from project plan; not complete:

| Area | RVAs (examples) | Status |
|------|-----------------|--------|
| TCP framing | `0xD84BB0`, `0xD84970` | Hooks written, **off** in `main.cpp` |
| `PNCliWorker` thread | `0xD668B0`, `0xD70A10`… | Not hooked |
| FSM driver | `0xD6F7B0` | States 1–3 hooked; driver itself not |
| Per-case `ProcessMessage` handlers | `0xD64F10`, … | Tail-call GAME from reimpl |
| `Message_Read` / stream | `0xD59300`, `0xD58B30` | Tail-call GAME |
| Game RMI after ProudNet | `0x4BA070` (`0x3F3E`) | Not hooked |

Cross-reference: [../proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md), [../proudnet-sdk-crossmap.md](../proudnet-sdk-crossmap.md) (PN v1.8 @ IDA **13338**).

---

## Verify recipe

```powershell
just ensure-serve          # once
just ctl::ping
just build-debug
just ctl::copy-dll
just ctl::launch-offline
just ctl::wait-stage shard_choice 240
just ctl::copy-logs
```

Inspect `ctl/logs/runs/<run_id>/events.jsonl` for `0xC0000005`, `0xC0000096`, `0xE06D7363`, and hook log prefixes (`pn_drain_recv:`, `pn_tcp_frame:`, …).
