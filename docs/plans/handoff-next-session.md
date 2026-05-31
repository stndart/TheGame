# Handoff prompt - next session (compaction seed)

> Paste this as the opening prompt for the next agent. It is self-contained; the
> linked docs/journals carry the detail. Written 2026-05-29 after run 177.

---

You are lead developer reimplementing the network/server side of **TheGame** (a ProudNet
v1.5 client, `GAME.exe`). **Goal: drive the offline client all the way to loading a
map.** We do this by reverse-engineering the **game RMI protocol** (not extending the
opaque FA-EMU replay) and generating the S2C responses the client needs, verifying each
step live with the controller. The DLL (`TheGame.dll`) is our microscope + verification
harness.

## Read first
- [docs/plans/proudnet-rmi-server-plan.md](proudnet-rmi-server-plan.md) - the living plan: strategy, the
  game-state→ctl-stage table (verified), S2C dispatch map, C2S capture findings, current frontier.
- [docs/proudnet/message-dispatch.md](../proudnet/message-dispatch.md) - `MessageType` ordinal switch
  (transport layer; do NOT confuse ordinals with RMI ids or wire opcodes).
- [docs/plans/proudnet-hook-status.md](proudnet-hook-status.md) - hook install matrix + SEH/GS prologue resume rules.
- AGENTS.md - workflow + the `just ctl::*` iterate loop. IDA Pro MCP (`user-ida-pro-mcp`,
  GAME.exe.i64 loaded, base 0x400000) and the controller daemon must both be up or you are blocked.

## State of the world (verified)
- Client reaches `shard_choice` → `server_ready` autonomously; a human can then navigate
  `lobby` → `room_list` etc. Stage hooks for every screen are wired and correct (the old
  deceptive `main_menu` is now `server_ready`); RVAs in the plan doc's stage table.
- **C2S RMI capture is on the game proxy and per-action ids are pinned (run 179).** Hooks in
  `src/hooks/net/pn_game_rmi_send_hook.cpp`: `sub_65AEA0` → `c2s_grmi proxy id=…` (16xxx band,
  explicit id); `sub_A0B290` → `c2s_grmi floor id=…` (15xxx+18xxx bands, id=`*(u16*)msg`).
  Both verified stable through a full nav pass, no faults. `0xD5C5E0` (`pn_rmi_send_hook.cpp`) is
  only a **framework heartbeat** trace (ids 1001/1006/1019) - NOT the game RMIs.
  The full proxy map + verified action→REQ table is in proudnet-rmi-server-plan.md (sections
  "Game RMI proxy map" and "C2S RMI capture - run 179").
- S2C RES handlers registered by `CAccountPacket::Register sub_4B8630` (~55 ids) and
  `CCommonPacket::Register sub_421070` (~43 ids). Convention: **REQ id = RES id − 1**.
  Anchors: connect `0x3F3E`@0x4BA070; room/match `0x3F81`@0x4BB450, `0x3F03`@0x4BB8F0,
  `0x3F11`@0x4BB7E0, `0x3F47`.

## Immediate next action
1. **DONE - per-action REQ ids pinned (run 179, `ctl/logs/runs/179_9e114d79/events.jsonl`).**
   The `c2s_grmi` capture lands in **`events.jsonl`** (NOT `game_logs.txt`, which is a partial
   early snapshot). Verified mapping (full table in proudnet-rmi-server-plan.md "run 179"):
   global chat "good morning" = floor `0x3AD2` (15058) len36; **Quick Dive** = proxy `0x3EE4`
   (16100) len3 (one emit per press); Custom-match list/refresh = proxy `0x3F2F` (16175);
   **Create Room** "rise of the sheep" = proxy `0x3F30` (16176) / floor `0x3AA0` (15008) len98.
   Earlier predictions `0x3ABF`/`0x3B24` were wrong (those are in-room screens we never reached);
   `0x3F40` is lobby-enter, not chat.
2. **Wall = Create Room** (`0x3F30`/`0x3AA0`, len98): list nav worked, but Create produced no
   transition. RE the 98 B REQ body (room name wstring + settings) and find its S2C RES handler
   in the registrar table (`sub_4B8630`/`sub_421070`). **Do NOT assume RES = REQ+1** - `0x3F2F`
   and `0x3F30` are both outbound REQs, so look the RES up explicitly.
3. Generate that RES as a plaintext `0x02`+id+body S2C frame in `server/server/`
   (proud_rmi.py / game_transport.py), replacing opaque replay one msg at a time. Fallback for
   any RES needing decrypted C2S content: in-DLL RMI injection (bypass the wire).

## Iterate loop (don't spawn duplicate daemons - `just ctl::ping` first)
```
edit C++/server → just build → just ctl::copy-dll
just ctl::launch-offline → (human navigates past lobby) → just ctl::wait-stage <stage> <timeout>
read ctl/logs/runs/<run>/events.jsonl  (+ FA-EMU proudnet_tcp.txt for wire frames)
just ctl::kill-all   # if daemon reports stale "Game is already running"
```
Benign in logs: `0x406D1388` (SetThreadName). Faults to watch: `0xC0000005`, `0xC0000096`, `0xE06D7363`.

## Gotchas
- Hook resume: never resume at RVA+5 on SEH/GS-prologue functions; relocate the real
  prologue bytes and resume past them (see existing hooks for the exact pattern).
- Capture C2S at the **in-process RMI layer**, never the wire - C2S is wrapped in transport
  `0x25` (reliable/compressed) and unreadable on the socket; S2C replay is plaintext `0x02`.
- Don't load the original engine source into context (huge); use semantic search / subagents.
- After non-trivial work: brief journal from `journals/template.md` + long mirror in `journals/long/`.
