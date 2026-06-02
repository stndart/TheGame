# Lobby navigation (handler pipe)

**TheGame.dll** drives UI transitions from ctl via the **handler named pipe** (`thegame-handler`, duplex). The elevated daemon accepts the game client; ctl sends line-oriented commands; the DLL replies with one line per request.

**Related:** [ctl/README.md](../../ctl/README.md), [client.md](client.md), [fake-server-hooks.md](fake-server-hooks.md) (removed inject), [UI automation journal](../../journals/long/2026-05-29-04-ui-automation-investigation.md).

---

## Handler pipe

| Direction | Payload |
| --- | --- |
| ctl → game | One UTF-8 line per command, `\n`-terminated |
| game → ctl | One response line |

| Command | Response | Main-thread effect |
| --- | --- | --- |
| `commands` | `nav_goto_lobby` (comma-separated list) | — |
| `nav_goto_lobby` | `ok` | Queue → send C2S `0x3F40` once (after `shard_select` already sent `0x3F0C` + `0x3E99`) |

**Threading:** a **reader thread** ([`handler_pipe.cpp`](../../src/diagnostics/handler_pipe.cpp)) enqueues work; **never** calls game code. **`NavPump`** ([`Nav.cpp`](../../src/RMI/Nav.cpp)) drains on the **main thread** via a `WH_GETMESSAGE` hook (installed after the first stage hook) plus `onPreProcess` stage hooks when the active state ticks.

**No env autonav:** `THEGAME_NAV_AUTO`, `THEGAME_NAV_ACTION`, and sidecar files are **ignored**. Use `just ctl::send` / MCP `ctl_send` only.

**No inject:** in-process S2C fake-server was removed for v1; the friends server is authoritative.

---

## ctl stages (current)

| `game_stage` | Hook RVA | Notes |
| --- | --- | --- |
| `shard_select` | `0x4347CC` | `CGameServer::onPreProcess` **end** — shard picker UI visible |
| `lobby` | `0x42BD50` | Main menu (Quick / Custom match) |
| `room_list` | `0x4362E0` | Custom match room list |
| `room` | `0x439B00` | Waiting room |

Hook @ `0x4345B0` (`CGameServer` begin) is installed but **does not** emit a ctl stage (RVA preserved).

---

## `nav_goto_lobby`

1. ctl sends `nav_goto_lobby` after **`shard_select`** (game already sent server-enter `0x3F0C` + `0x3E99` at that stage).
2. On next main-thread `NavPump`: if not in fade lock, send lobby-enter notify `0x3F40` once.
3. Friends server answers (e.g. S2C `0x3F41`); client reaches scene 4 → **`lobby`** stage.

Does **not** call `RequestState(4)` or inject RES — server drives the transition.

Logs: `[nav] command nav_goto_lobby`, `[nav] c2s 0x3F40`, `[rmi] s2c id=0x3F41`, `[stage] lobby`.

---

## UI handler pattern (future commands)

Preferred automation: call real GAME click handlers (e.g. `sub_42CAE0` `onClickRoomList` with null `MsgDelegateArg*`) rather than raw C2S or GFx binder detours (`sub_554B60` / `sub_556500`). See the UI journal Tier D table.

`Nav.cpp` contains a commented `invoke_ui_handler` stub for future `nav_click_room_list` commands.

---

## Verification (online)

```powershell
just build
just ctl::copy-dll
just ctl::launch
just ctl::wait-stage shard_select 120
just ctl::send nav_goto_lobby
just ctl::wait-stage lobby 120
just ctl::copy-logs
```

Or: `just ctl::run-e2e-lobby` (daemon must be up).

Grep `events.jsonl` for `[stage] shard_select`, `[nav] command nav_goto_lobby`, `[rmi] c2s proxy id=0x3F40`, `[stage] lobby`.

**Shard pick:** if the UI requires choosing a shard, do that after `shard_select` before `nav_goto_lobby`.

---

## `DISABLE_AUTONAV` (legacy, no-op)

Compile default OFF. **`NavPump` always runs** — handler-pipe commands are not gated by this flag.

`ctl launch` merges `DISABLE_AUTONAV=0` into the launcher env to override a stale shell value.

**Shard UI:** ctl `shard_select` dedupes to one event; `CGameServer` may not be the ticking state while idle on the picker. `NavSchedulePump` installs the get-message hook and posts `WM_NULL` to the main thread captured from stage hooks.

---

## RMI trace hooks (logging only)

| Hook | RVA | Log line |
| --- | --- | --- |
| `hook_pn_rmi_send` | `0xD5C5E0` | `[rmi] c2s framework …` |
| `hook_pn_game_rmi_send` | `0x65AEA0` | `[rmi] c2s proxy …` |
| `hook_pn_rmi_floor` | `0xA0B290` | `[rmi] c2s floor …` |
| `ProcessMessageProudNetLayer` case Rmi | `0xD653B0` | `[rmi] s2c …` |
