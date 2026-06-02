# UI autonav: shard_select → lobby (long)

**Date:** 2026-06-02 (re-verified 2026-06-03)  
**Runs:** `302_c1ebe23c`, `304`-`307` (nav + `0x3F40`, no lobby), `309` (baseline), `310_1bc6c166` (first success), `311_ef296d2a` (clean e2e), **`320_3807cf48`** (`nav_goto_lobby` only — no lobby), **`321_1751c04e`** (`nav_pass_shard_select` only — lobby)  
**Code:** [`src/RMI/Nav.cpp`](../../src/RMI/Nav.cpp), [`src/diagnostics/handler_pipe.cpp`](../../src/diagnostics/handler_pipe.cpp), [`ctl/justfile`](../../ctl/justfile)  
**Prior art:** [2026-05-29-04-ui-automation-investigation.md](2026-05-29-04-ui-automation-investigation.md) (RequestState, click handlers, “no WM_* path”)

---

## 1. Problem statement

After RMI v1 cleanup (removed fake-server inject, unified `[rmi]` logging, renamed ctl stage **`server_ready` → `shard_select`**), the e2e target was:

1. Launch online (`just ctl::launch`).
2. Wait **`shard_select`**.
3. Send a **handler-pipe message** (no inject).
4. Wait **`lobby`**.

`nav_goto_lobby` was the only command. Automation repeatedly stopped at **`shard_select`** even when logs showed C2S traffic.

---

## 2. Game flow (IDA) — what “pass shard” actually means

### 2.1 ctl stage `shard_select` (`0x4347CC`)

Hook at **end** of `CGameServer::onPreProcess` (`sub_4345B0`). When byte at `dword_1C10D80 + 128` is clear, the game sends server-enter **`0x3F0C`** + **`0x3E99`** before the picker is fully idle. **Our nav must not re-send that pair** — it is already done when ctl fires `shard_select`.

### 2.2 User clicks join / picks shard (`sub_434F20` @ `0x434F20`)

GFx: `onClickJoinServer`. Decompiled branch when server index from UI ≠ cached selection (`this+34`):

```cpp
*(_WORD *)v4 = 15051;           // floor 0x3AC3
*(_DWORD *)&v4[2] = server_index;
sub_65AEA0(v4, 6, 16010);       // proxy RMI 0x3EB2
```

If index **already equals** selection → GFX path only (`onGFxClickJoinServer - LOBBY`), **no** `0x3EB2.

**Lesson:** automation must send **`0x3EB2`** with the intended index (or invoke the real click handler with valid `this`), not only a lobby notify.

### 2.3 Server farm change RES (`sub_4354D0` @ `0x4354D0`)

Bound from shard UI registration (`sub_434B50`); log string `"onNetChangeServerFarmRES - LOBBY"`. On success (`u16` at message body +2 == 0):

- Updates `this+34`, `this+35`, `word_1C1AC00`, `dword_1C1ABFC`, `byte_1C1D578`, etc.
- If **`!byte_1C1E409`** (not in fade lock) and **`dword_1C15644 != 4`**:
  - `sub_49E530` / `sub_40B340` (fades)
  - **`sub_41F0D0(&dword_1C155C0, 4)`** — **RequestState → lobby scene**
- `sub_63E9B0("setServerListInfoHide", ...)` and follow-up UI refresh

**Lesson:** reaching **`lobby` stage** is tied to **scene id 4** (`dword_1C15644`), not to an arbitrary C2S notify.

### 2.4 Lobby enter notify (`0x3F40`)

Separate step (floor **`0x3ACE`**, 2 B). Successful captures (e.g. run ~294) show this **after** the client is in the lobby flow, not as the only action from the shard list.

### 2.5 Lobby stage hook (`0x42BD50`)

ctl emits **`lobby`** when main-menu lobby state runs — **after** scene/UI transition, not when a lone packet leaves the wire.

---

## 3. Investigation timeline (runs)

### Phase A — command never executed (run **302**)

**Symptoms:** `[nav] enqueued nav_goto_lobby` on pipe reader TID; **no** `[nav] command nav_goto_lobby`.

**Cause:** [`handler_pipe.cpp`](../../src/diagnostics/handler_pipe.cpp) reader thread enqueues; **`NavPump` only ran from stage hooks**. On shard picker the game is **idle** — `CGameServer::onPreProcess` does not fire every frame; ctl **`shard_select` is deduped once**.

**Fix (still required for any nav command):**

- `NavSchedulePump()` after enqueue.
- Capture main TID from first stage hook; install **`WH_GETMESSAGE`** hook to call `NavPump`.
- Wakeup thread: **`PostThreadMessage(WM_NULL)`** every 500 ms while nav pending.

**Lesson:** *“Enqueued” ≠ “ran on main thread.”* Always confirm **`[nav] command ...`** in `events.jsonl`.

### Phase B — command runs, lobby never reached (runs **304-307**)

**Symptoms:** `[nav] command nav_goto_lobby`, `[nav] c2s 0x3F40`, sometimes `[rmi] s2c ...`; **`wait-stage lobby` timeout**.

**Mistake:** Interpreted as “server doesn’t answer” or “need inject” first.

**Actual issue:** **`nav_goto_lobby` only calls `send_lobby_enter_notify()`** (`0x3F40`). Client still on **shard picker scene** — no **`0x3EB2`**, no **`RequestState(4)`**.

**Lesson:** Separate **transport success** from **UI state machine success**. Grep **`[stage] lobby`**, not only `[nav] c2s`.

### Phase C — wrong C2S earlier in the same effort

Before restricting to `0x3F40`, nav re-sent **`0x3F0C` + `0x3E99`** at shard stage.

**Mistake:** Treated “lobby navigation” as “repeat server-enter.”

**Lesson:** Read **what the stage hook already did** (`0x4347CC` auto-send). Match **reference run wire order**, not generic “enter server” IDs from docs tables.

### Phase D — working path (runs **310**, **311**)

**Command:** `nav_pass_shard_select` — pump order:

1. **`0x3EB2`** once (index from `THEGAME_NAV_SHARD_INDEX`, default 0).
2. When fade lock clear and scene ≠ 4: **`sub_41F0D0(&dword_1C155C0, 4)`** once.
3. When scene == 4: **`0x3F40`** once → clear pending.

Run **310** `events.jsonl` (abridged):

```text
[nav] command nav_pass_shard_select
[nav] c2s 0x3EB2 shard index=0
[nav] RequestState scene=4
[stage] lobby
[nav] c2s 0x3F40 lobby enter
[nav] pass_shard_select done
```

Run **311:** full `just ctl::run-e2e-lobby` exit 0.

### Phase E — re-verification (runs **320**, **321**, 2026-06-03)

Controlled replay after journal review: same online launch, **`shard_select`**, 8 s settle, **one** nav command, **`wait-stage lobby`**.

| Run | Command | Result |
| --- | --- | --- |
| **320** | **`nav_goto_lobby`** only | `command` + **`c2s 0x3F40`** (×2 while pending); **`game_stage` stays `shard_select`** until session ended; **no `lobby`** from goto alone. Later manual **`nav_pass_shard_select`** in same session → **`lobby`**. |
| **321** | **`nav_pass_shard_select`** only | `0x3EB2` → `RequestState scene=4` → **`game_stage` `lobby`** → `0x3F40` → `pass_shard_select done`; **`wait-stage lobby` OK** (~58 s). |

Run **321** `events.jsonl` (abridged):

```text
[nav] command nav_pass_shard_select
[nav] c2s 0x3EB2 shard index=0
[nav] RequestState scene=4
game_stage lobby
[nav] c2s 0x3F40 lobby enter
[nav] pass_shard_select done
```

**Lesson (confirmed):** For ctl e2e from **`shard_select`**, treat **`nav_goto_lobby` as non-functional** — it may put **`0x3F40`** on the wire but does **not** advance the UI state machine. Use **`nav_pass_shard_select`** exclusively.

---

## 4. What `nav_goto_lobby` does (and why it failed e2e)

Implementation: [`pump_goto_lobby()`](../../src/RMI/Nav.cpp)

| Condition | Behavior |
| --- | --- |
| `dword_1C15644 == 4` | Clear pending, log `goto_lobby done`, **send nothing** |
| `byte_1C1E409` set | Wait, log `goto_lobby blocked` |
| Else | Send **`0x3F40`** once (does not clear pending until scene == 4) |

**Wire vs UI:** It is **not** a transport no-op — run **320** shows **`c2s 0x3F40`**. For automation it is **effectively a no-op**: **`game_stage` never leaves `shard_select`** and **`wait-stage lobby` never succeeds** (runs **304–307**, **320**).

From **`shard_select`** it is the **wrong step**: lobby-enter notify without **`0x3EB2`** or **`RequestState(4)`**.

**Naming trap:** “goto_lobby” sounds like the full shard→lobby journey; implementation is **“retry lobby-enter notify while scene ≠ 4.”** Do **not** use it in e2e or ctl recipes.

---

## 5. Implementation notes

### 5.1 In-process `RequestState` vs server S2C

We do **not** call `sub_4354D0` or synthesize farm-change RES. We call **`RequestState(4)`** directly after **`0x3EB2`**, on the **main thread**, when fade lock allows.

**Works** on current friends-server online e2e (311). **Risk:** missing globals/fades `sub_4354D0` sets (`word_1C1AC00`, `byte_1C1D578`, ...) may break other flows (offline dummy, second shard change, UI polish).

**Lesson:** Prefer **mirroring the handler that runs on success**, not only the last packet in a capture. Long-term options: (a) dummy S2C for `0x3EB2`, (b) call `sub_434F20` with real binder `this`, (c) minimal fake `CReceivedMessage` into `sub_4354D0`.

### 5.2 Main thread (repeated from UI journal)

`sub_41F0D0` must run on the **game main thread** ([UI journal §3](2026-05-29-04-ui-automation-investigation.md)). Handler pipe + get-message hook satisfy this; **never** call from pipe reader.

### 5.3 GFx binder id vs RMI id

`sub_434B50` registers callbacks with numeric ids (e.g. **16010**). Same number appears as **proxy RMI 0x3EB2** in the click path — easy to confuse with “S2C id 16010 on wire.” Trust **`sub_65AEA0`** call sites for wire IDs.

---

## 6. ctl / e2e mistakes

### 6.1 `last_run.json` stale during session (run **310**)

`write_last_run` ran only in **`end_session()`** (after kill/copy-logs). E2e **`verify-nav-plumbing`** read `last_run.json` → still pointed at run **309** → **false failure** while run **310** had full nav success.

**Fix:** `write_last_run` in **`begin_session()`**; verify uses **`uv run ctl status`** → `run_dir`.

**Lesson:** Diagnostics recipes must target the **active session**, not “last finished” run.

### 6.2 verify patterns vs log format

Nav logs: `[nav] command nav_pass_shard_select (tid=...)`. Verify uses substring `command nav_pass_shard_select` — OK.

Do not verify only `enqueued` — that was the false positive in run **302**.

### 6.3 `DISABLE_AUTONAV` / stage hook

Early builds pumped from `diagnostics_game_stage_shard_select` when autonav enabled. Final design: **pipe-only**; stage hook only calls `NavPump` for main-TID capture / optional co-drain, not auto-send. Boot may still log `autonav_off=1` from env; **nav commands are not gated** on that flag in the shipped fix.

---

## 7. Mistakes & lessons learned (checklist for next agent)

1. **Map commands to the full UI sequence** — shard picker needs **`0x3EB2` + scene transition + notify**, not one ID from a table.
2. **Stage hooks are sparse** — deduped `shard_select` is not a frame loop; use **message pump hook** for idle GFx screens.
3. **Prove execution, not enqueue** — require `[nav] command ...` line.
4. **Prove outcome, not C2S** — require `[stage] lobby` (or equivalent in-game invariant).
5. **Read IDA click + RES pair** — REQ (`434F20`) and success handler (`4354D0`) before naming ctl commands.
6. **Don’t duplicate stage-hook sends** — `0x3F0C`/`0x3E99` at `shard_select` is already game-owned.
7. **Don’t reach for inject first** — `RequestState` + correct C2S was enough online; inject docs remain for true S2C/offline gaps.
8. **Align verify with session lifecycle** — `status.run_dir` or explicit `run_id` from `launch` JSON.
9. **Do not use `nav_goto_lobby` for shard→lobby** — re-verified run **320**; e2e / ctl default is **`nav_pass_shard_select`** only.
10. **`nav_goto_lobby` when already scene 4** — clears pending without sending; legacy narrow helper, not part of the working path.

---

## 8. Command reference (current)

| Command | Use when |
| --- | --- |
| **`nav_pass_shard_select`** | At **`shard_select`**: full automation to **`lobby`** (e2e default) |
| **`nav_goto_lobby`** | **Deprecated for automation** — notify-only, no shard→lobby transition (run **320**) |
| `THEGAME_NAV_SHARD_INDEX` | Non-default shard row in server list |

Pipe list: `just ctl::commands` → `nav_goto_lobby,nav_pass_shard_select`.

---

## 9. Follow-ups

| Item | Why |
| --- | --- |
| Dummy server S2C for `0x3EB2` | Offline `launch-dummy` may need server-driven `sub_4354D0` path |
| `nav_click_join_server` | Call `sub_434F20` with binder `this` (`registration this-92` from `sub_434B50`) |
| Fade + globals before `RequestState` | Closer parity with `sub_4354D0` if transitions flake |
| Multi-shard / private rooms | Index env may be insufficient |

---

## 10. Related grep patterns

```text
[stage] shard_select
[nav] command nav_pass_shard_select
[nav] c2s 0x3EB2
[nav] RequestState scene=4
[stage] lobby
[nav] c2s 0x3F40
[rmi] s2c
```

E2e: `just ctl::run-e2e-lobby` (daemon up: `just ctl::ping`).
