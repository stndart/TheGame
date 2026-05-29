# Long journal: per-action C2S RMI ids from run-179 nav pass

**Date:** 2026-05-29
**Run:** `ctl/logs/runs/179_9e114d79/` (`events.jsonl`, `game_proudnet_tcp.txt`, `game_logs.txt`, `meta.json`)

## Session facts (meta.json)

- started 07:02:46Z, ended 07:04:32Z, 79 events, launcher_pid 37612, game pid 36216.
- game_states: started → intro → connecting_to_server → shard_choice → server_ready → lobby →
  room_list → lobby → shard_choice → server_ready (→ disconnected 07:04:31).
- step timestamps: intro 07:02:58, connecting 07:03:49, shard_choice 07:03:53.76, server_ready
  07:03:53.86, lobby 07:03:59.38, room_list 07:04:13.63, lobby 07:04:23.36, shard_choice
  07:04:24.46, server_ready 07:04:24.56, disconnected 07:04:31.39.

Player narration (from user): connected, wrote "good morning" to global chat, pressed Quick Dive
twice, tried to create "rise of the sheep" custom match, exited.

## Where the capture lives (gotcha)

`game_logs.txt` for run 179 (and 178) is only ~28 lines and ends at the connection phase
("DLL unloaded" / "pn_drain_recv"). It contains **no** `c2s_grmi` lines. It is a partial
early-flushed snapshot, not the full log. The canonical full stream is **`events.jsonl`**.

`emit_game_log` (grmi lines) and `emit_proudnet_tcp` (wire frames) both write the same named
pipe via `Diagnostics::*` (`src/diagnostics/handlers.cpp`). `game_proudnet_tcp.txt` has 115
frames spanning the whole session, proving the pipe was healthy throughout - so the missing
grmi lines in `game_logs.txt` are a snapshot artifact, not a capture failure.

The grmi hooks are registered in `src/main.cpp:94-95`
(`g_target_pn_game_rmi_send` @ `sub_65AEA0`, `g_target_pn_rmi_floor` @ `sub_A0B290`). Log format
(`pn_game_rmi_send_hook.cpp`): `c2s_grmi proxy id=0x%04X (%u) len=%u` and `... floor ...`.
Proxy id = stack arg `rmiId:int16`; floor id = `*(u16*)msg` (first LE u16 of the message buffer).

## Full grmi timeline (events.jsonl line → reading)

LOGIN / handshake burst (before shard_choice, lines 22, 35–47) - not user actions:
```
22 floor 0x4654 (18004) len=1565    initial auth/session payload (18xxx band)
35 floor 0x3ACA (15050) len=10
36 floor 0x3AC8 (15048) len=1348
37 proxy 0x3F20 (16160) len=2
38 floor 0x3AFA (15098) len=2
39 proxy 0x3F25 (16165) len=2
40 floor 0x3AFC (15100) len=2
41 floor 0x3B03 (15107) len=203
43 floor 0x3AC6 (15046) len=10246   big state/asset sync
44 proxy 0x3EB5 (16053) len=2
45 floor 0x3AF3 (15091) len=2
46 proxy 0x3F37 (16183) len=10
47 floor 0x3AC3 (15043) len=10
```

SERVER SELECT (shard_choice@48 / server_ready@49):
```
50 proxy 0x3F0C (16140) len=2
51 floor 0x3AD1 (15057) len=2
52 proxy 0x3E99 (16025) len=2
53 floor 0x3AD4 (15060) len=2
54 c2s_rmi 0x03E9 (1001)             framework heartbeat (0xD5C5E0 trace)
```

LOBBY (game_state lobby@55):
```
56 proxy 0x3F40 (16192) len=2 \  lobby-enter REQ (gated onPreProcess)
57 floor 0x3ACE (15054) len=2 /  (same RMI, two layers)
58 EXCEPTION 0x000006BA            RPC_S_SERVER_UNAVAILABLE - benign, not a crash
59 floor 0x3AD2 (15058) len=36     CHAT "good morning" (standalone floor, text body)
60 proxy 0x3EE4 (16100) len=3 \  Quick Dive press #1
61 floor 0x3AAF (15023) len=3 /
62 proxy 0x3EE4 (16100) len=3     Quick Dive press #2
```

ROOM_LIST (game_state room_list@63):
```
64 proxy 0x3F2F (16175) len=2 \  enter custom-match list / list req
65 floor 0x3A9F (15007) len=2 /
66 c2s_rmi 0x03E9 (1001)           heartbeat
67 proxy 0x3F2F (16175) len=2     list refresh
68 proxy 0x3F2F (16175) len=2     list refresh
69 proxy 0x3F30 (16176) len=98 \ CREATE ROOM "rise of the sheep"
70 floor 0x3AA0 (15008) len=98 /  (name wstring + room settings)
71 proxy 0x3F2F (16175) len=2     list refresh / back
```

EXIT (lobby@72 → shard_choice@73 → server_ready@74):
```
75 proxy 0x3F0C (16140) len=2 \  leave lobby → server select (mirror of 50/51)
76 floor 0x3AD1 (15057) len=2 /
77 floor 0x3ACD (15053) len=6
78 c2s_rmi 0x03EE (1006)           heartbeat
79 DLL unloaded
```

## Pairing logic

Wrapper `sub_65AEA0` (proxy) tail-calls floor `sub_A0B290`. So a proxy line immediately followed
by a floor line of equal len is ONE RMI observed at both layers (proxy id = app id in the 16xxx
band; floor id = the serialized inner id in the 15xxx band). Pairs seen: 0x3F40↔0x3ACE,
0x3EE4↔0x3AAF, 0x3F2F↔0x3A9F, 0x3F30↔0x3AA0, 0x3F0C↔0x3AD1, etc. The id deltas are not constant
(0x472, 0x435, 0x490, 0x490, 0x43B) → two distinct id spaces, not a fixed offset.

A floor line with NO preceding matching proxy = a standalone inline sender (calls `sub_A0B290`
directly, bypassing the wrapper). `0x3AD2` (chat) is the key example: line 56 proxy `0x3F40`
already paired with line 57 floor `0x3ACE`, so line 59 floor `0x3AD2` len36 stands alone → it is
its own message, the only one carrying a text-sized body in the lobby phase → chat "good morning".

## Chat length sanity check

"good morning" = 12 chars. UTF-16LE = 24 B. Floor `len=36` ≈ 2 B inner id + 4 B wstring length
prefix + 24 B text + ~6 B channel/type fields. Consistent. To confirm: retype a distinctly
longer string and watch the floor len scale by 2 B/char.

## Corrections vs predictions (proudnet-rmi-server-plan.md "Action → REQ id")

- chat: predicted `0x3F40` - WRONG (that is lobby-enter). Actual = floor `0x3AD2` (15058).
- Quick match: predicted `0x3ABF` (15039) - not seen. Lobby Quick Dive = proxy `0x3EE4` (16100).
  `0x3ABF` is the in-room automatch start (`sub_43C7E0`), a screen we did not reach.
- Create: predicted `0x3B24` (15140) - not seen. Custom create = proxy `0x3F30` (16176) / floor
  `0x3AA0` (15008). `0x3B24` is the party-room create (`sub_42F690`), a different screen.
- room-list `0x3F2F` (16175) and lobby `0x3F40` (16192): CONFIRMED as predicted.

## Behaviour / next step

`lobby→room_list` transition happened (list nav is client-local or the replay answered).
Quick Dive (`0x3EE4`) and Create (`0x3F30`) produced no forward transition - replay has no RES,
so the player backed out (room_list→lobby) and exited (→shard_choice→server_ready→disconnect).

Wall = Create Room. Next: RE the 98 B `0x3F30`/`0x3AA0` REQ body (room name + settings: map,
mode, max players, password?), locate its S2C RES in the registrar tables (`sub_4B8630`
CAccountPacket / `sub_421070` CCommonPacket). REQ=RES−1 is NOT reliable here because `0x3F2F`
and `0x3F30` are both outbound REQs - look the RES id up explicitly. Then emit that RES as a
plaintext `0x02`+id+body frame in `server/server/` (proud_rmi.py / game_transport.py).
