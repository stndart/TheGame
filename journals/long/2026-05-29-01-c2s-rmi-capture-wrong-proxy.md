# Long: C2S RMI capture lands on the framework proxy, not the game proxy

**Date:** 2026-05-29
**Run:** `ctl/logs/runs/177_e1479e0f/` (pid 2648)

---

## Raw capture (run 177 events.jsonl, c2s_rmi + phase lines)

```
phase: started → intro → connecting_to_server
log: c2s_rmi id=0x03EE (1006) remotes=1        # during connect
phase: shard_choice → server_ready
log: c2s_rmi id=0x03E9 (1001) remotes=1  x many   # poll
log: c2s_rmi id=0x03FB (1019) remotes=1  (periodic, interleaved)
phase: lobby                                       # the real "main menu"
   ... 0x03E9 frequent, 0x03FB periodic ...
phase: room_list                                   # Custom Match list
   ... same two ids only ...
phase: lobby → shard_choice → server_ready         # user returned to menu/shard
   ... same two ids only ...
```

User actions performed in this window (per user report): selected server; typed "Hi hi hi"
into global chat (never echoed); Quick Match ×~4; Custom Match → Create → named "Wolves" →
confirm (popup vanished, no transition); refresh list; back to menu; back to shard select.

None of those actions introduced a new rmi id at 0xD5C5E0.

## Why this is the framework proxy

- The id magnitudes (1001/1006/1019) are far below the game RES band (0x3Exx/0x3Fxx ≈
  16000s). ProudNet reserves low RMI ids for its built-in proxies.
- `0x03E9` (1001) cadence == heartbeat/poll; `0x03FB` (1019) periodic == likely server-time
  or state poll; `0x03EE` (1006) once at connect == a framework connect/notify RMI.
- The game's own account/lobby/room RMIs must therefore use a different proxy stub. ProudNet's
  PIDL compiler generates a `<Name>Proxy` class alongside the `<Name>Stub`; we have the Stub
  registrars (CAccountPacket::Register sub_4B8630, CCommonPacket::Register sub_421070) but not
  the Proxy send.

## Hook details (current, kept as heartbeat trace)

`src/hooks/net/pn_rmi_send_hook.cpp` - target RVA 0xD5C5E0, resume 0xD5C5E7.
SEH/GS prologue relocated: `push -1; push 0x1511D84`. Reads at stub entry (+0x24 for
pushad+pushfd): RMIId [esp+0x3C] (entry [esp+0x18]), remoteCount [esp+0x2C] (entry [esp+0x08]).
Stable across full session.

## Open IDA questions (dispatched to subagent 2026-05-29)

1. Identity of 1001/1006/1019 - proxy class + framework vs game.
2. get_xrefs_to 0xD5C5E0 - universal core or proxy-specific? Is there a lower core send the
   0x3Fxx RMIs use?
3. Locate CAccountProxy/CCommonProxy send chokepoint (emits 0x3Fxx).
4. Trace UI handlers: chat send (CGameLobby 0x42BD50), Quick Match (CGameLobby), Create-Room
   confirm (CGameRoomList 0x4362E0) → REQ ids.

## Next steps

- Move capture to the game proxy; recapture button REQ ids with ground truth.
- For each REQ, RES = REQ+1; find handler RVA in the registrar tables; RE body layout.
- Generate plaintext `0x02`+id+body S2C frames in server/server/ (proud_rmi.py,
  game_transport.py), replacing opaque replay incrementally. Fallback: in-DLL RMI injection.
