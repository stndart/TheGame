# Game RMI - server (data + implementation)

> **Separate package:** The friends **dummy server** is not part of this RE repo (submodules removed). Paths below describe that server’s layout for cross-reference when integrating S2C builders with `TheGame.dll` + **ctl**.

How the **Python dummy server** builds S2C game RMI frames and what replay artifacts remain opaque.

---

## Wire format

One GAME-leg **burst** = concatenated Proud TCP frames; each inner payload:

```text
[0x02][rmi_id:2 LE][body...]
```

Builders: [`server/server/proud_rmi.py`](../../server/server/proud_rmi.py), framing in `proud_frame.py`, triggers in `game_transport.py`.

---

## S2C builders

| RMI | Builder | Min body | Leaf / effect |
| --- | --- | --- | --- |
| `0x3F3E` | `build_net_user_connect_res_frame()` | see § NetUserConnectRES | `sub_4BA070` - connect |
| `0x3F30` | `build_create_room_res_frame()` | 4 B (`result@+2==0`) | `sub_437160` → scene 9 |
| `0x3ED4` | `build_room_enter_res_frame()` | 6+32×N | `sub_4BB560` |
| `0x3ED8` | `build_room_members_ui_res_frame()` | 6+168×N | `sub_4BB370` |
| `0x3F3D` | `build_start_match_res_frame()` | 0x40 B | `sub_43D9B0` → scene 11 |
| `0x3F45` | `build_leave_room_res_frame()` | 4 B (WIP) | `sub_43D020` |
| `0x3E8E` | `build_net_connect_res_frame()` | 64 B stub | WIP |
| `0x3E99` | `build_notify_15060_frame()` | 6 B stub | WIP |

**Create-room burst:** `build_create_room_res_burst()` → `0x3F30` + `0x3ED4` + `0x3ED8` (242 B). Send as **one TCP write** (`CREATE_ROOM_RES_AS_BURST=true`).

---

## NetUserConnectRES (RMI `0x3F3E`)

**When:** Inside GAME lobby replay bursts; synthetic fallback if replay missing.

**Handler:** `sub_4BA070` (`onNetUserConnectRES`). Struct: [`include/game/net/net_user_connect_res.hpp`](../../include/game/net/net_user_connect_res.hpp).

| Offset | Size | Field | Semantics |
| --- | --- | --- | --- |
| `0x000` | 2 | pad | Ignored |
| `0x002` | 2 | `error_code` | **0 = success** |
| `0x004` | 4 | `farm_select_id` | uint32 |
| `0x008` | 4 | `session_uid` | Not account id |
| `0x010` | 76 | `name` | wchar[38] |
| `0x05C` | 4 | `field_5c` | Farm selector |
| `0x060` | 4 | `field_60` | Player slot index |
| `0x064` | 0x88 | `unk_block` | Zeros work offline |
| `0x0EC` | 4 | **`account_id`** | **Required** |
| `0x0F0` | 120 | `farm_slots[3]` | 3×40 B |
| `0x16C` | 4 | `num_char_slots` | |
| `0x170` | 16 | pad | |

### Body size (client vs server)

| Source | Size | Notes |
| --- | --- | --- |
| Client handler / [`net_user_connect_res.hpp`](../../include/game/net/net_user_connect_res.hpp) | **384 B** (`0x180`) | What this repo’s struct documents and safe emit uses |
| Friends-server replay capture | Often cited as **644 B** | May be the same field layout with trailing padding — **not confirmed** as a second schema; treat 384 B as authoritative for client fields until wire diff proves otherwise |

Optional `name=` patches wchar @ +0x10 in replay tooling.

---

## Server triggers (C2S is opaque inside `0x25`)

Server cannot parse C2S RMI from the socket - use **body length heuristics** on `0x25` session blobs or session counters. Config in `server/.env` / [`config.py`](../../server/server/config.py).

| Trigger env | Purpose |
| --- | --- |
| `GENERATE_CREATE_ROOM_RES_ON_REQ` | Wire create-room burst |
| `CREATE_ROOM_RES_ON_25_BODY_LEN=115` | Match create REQ blob length |
| `CREATE_ROOM_RES_ON_SESSION_N` | Nth session after lobby |
| `GENERATE_START_MATCH_RES_ON_REQ` | Start-match RES |
| `START_MATCH_RES_AFTER_CREATE_NTH_25=2` | After create, on Ready |
| `GENERATE_LEAVE_ROOM_RES_ON_REQ` | Leave-room RES |
| `LEAVE_ROOM_RES_ON_25_BODY_LEN=19` | Collides with enter/ready - prefer session counters |

**Verify:** friends server on the wire + `just ctl::copy-logs` (`[rmi] s2c` in netlogs). No in-process inject.

```powershell
cd server && uv run python -m server.test_wire_create_room
cd server && uv run python -m server.test_wire_start_match
```

---

## Lobby replay

| Artifact | Format | Sizes / notes |
| --- | --- | --- |
| `lobby_replay.json` | 3 hex strings (bursts) | 7323 + 2920 + 1193 B |
| `lobby_postinit.json` | 9 hex strings | Post-burst schedule |

**Runtime default:** full JSON bursts (`LOBBY_USE_CONSTRUCTED_FRAMES=false`). Unit tests use capture-exact builders (`test_wire_lobby_rmis.py`).

Inventory: `uv run python -m server.tools.scan_lobby_replay` from `server/`.

---

## Opaque blobs {#opaque-blobs}

Material that must stay **byte-identical** (or be replaced with a fresh capture from the same client build).

### `PN_KEY_BLOB` (S2C `0x04`)

| Property | Value |
| --- | --- |
| Size | 182 B after opcode (`proud_rmi.py`) |
| Origin | FA-EMU / live capture |
| If wrong | Client never sends `0x05` |

### Client C2S blobs (`0x05`, `0x07`, `0x25`)

Server accepts any payload and advances FSM - semantics unknown.

### `lobby_replay.json` bursts

Concatenated complete Proud frames. UI strings (**“Tom_Neverwinter”**, etc.) come from capture, not `SERVER_NAME` alone.

### ENTRY server-info prefix/postfix

Only **36-byte UTF-16 name** is computed from config; prefix/postfix frozen from capture.

### Stub follow-ups (`0x3E8E`, `0x3E99`)

64 B / 6 B placeholders - real session uses payloads inside lobby replay.

---

## Replacing FA-EMU artifacts

1. Capture trace from known-good client build.
2. Split into Proud frames (`13 57` boundaries).
3. Replace one artifact at a time.
4. Re-run `just ctl::launch-offline`; confirm `main_menu` / target stage.
5. Update `server/server/data/` or `proud_rmi.py`.

---

## Server file index

| Component | Path |
| --- | --- |
| ENTRY FSM | `server/server/entry_transport.py` |
| GAME FSM + triggers | `server/server/game_transport.py` |
| RMI builders | `server/server/proud_rmi.py` |
| Framing | `server/server/proud_frame.py` |
| Replay data | `server/server/data/*.json` |
| Wire tests | `server/server/test_wire_*.py` |

*Last updated: 2026-05-29.*
