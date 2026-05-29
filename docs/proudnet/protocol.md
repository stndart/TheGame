# ProudNet - wire protocol (data)

End-to-end **transport** bytes for the offline dummy server (`127.0.0.1`, ports **7000 / 27380 / 20009**). Normative reference for framing and transport opcodes. Game RMI body layouts: [../rmi/server.md](../rmi/server.md), [../rmi/client.md](../rmi/client.md).

**Scope:** TCP framing and transport FSM - not a complete ProudNet 1.7 specification.

**Evidence:**

| Source | Trust |
| --- | --- |
| Client hooks + IDA (`sub_D84BB0`, framing) | High for IDA-confirmed structures |
| `ctl/logs/runs/…`, `game_netlogs.txt` | High for ordering and sizes |
| `GITS-FA-emulation/` → `server/server/data/*.json` | Convenience replay only - see [§ opaque blobs](../rmi/server.md#opaque-blobs) |
| Synthetic fallbacks in `proud_rmi.py` | Low |

**Related:** [overview.md](overview.md) (intent), [../proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md) (RVAs).

---

## 1. Terminology

| Term | Definition |
| --- | --- |
| **Proud TCP frame** | Length-prefixed message; magic `13 57`. |
| **Inner payload** | Bytes inside a frame after length decode; first byte = **transport opcode**. |
| **Transport opcode** | ProudNet connection FSM byte (`0x2F` hello, `0x04` key, …). Distinct from **RMI id** and **MessageType** ordinal. |
| **RMI carrier** | Transport opcode `0x02` → `[0x02][rmi_id:2 LE][body]`. |
| **ENTRY / GAME / PROBE** | TCP legs on ports 7000 / 27380 / 20009. |
| **Burst** | One S2C byte string (may contain multiple concatenated Proud frames) after client `0x25` on GAME leg. |

**Endianness:** little-endian unless noted. **Strings:** UTF-16LE unless ASCII.

---

## 2. Proud TCP frame

Client parser `sub_D84BB0`, sender `sub_D84970`; server `proud_frame.py`, `include/game/net/proud_frame.hpp`.

### 2.1 Layout

| Offset | Size | Field | Description |
| --- | --- | --- | --- |
| 0 | 2 | `magic` | `0x57 0x13` (uint16 LE `0x5713`). |
| 2 | 1 | `len_tag` | Width of length field: `1`, `2`, or `4`. |
| 3 | `len_tag` | `payload_len` | Unsigned inner payload length. |
| 3+len | `payload_len` | `payload` | Transport message (§3). |

### 2.2 Length encoding

| `len_tag` | Used when |
| --- | --- |
| `1` | `payload_len ≤ 0x7F` |
| `2` | Medium frames |
| `4` | Large frames (e.g. big `0x25` session blobs) |

**Parser rule:** buffer until `3 + len_width + payload_len` bytes; multiple frames may arrive in one `recv()`.

### 2.3 Example (client hello)

```text
13 57 01 05  2F 0F 00 00 40
│  │  │  │   └─ inner payload (5 bytes)
│  │  │  └──── payload_len = 5
│  │  └─────── len_tag = 1
│  └────────── magic
```

---

## 3. Inner payload (transport opcodes)

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | `opcode` |
| 1 | rest | `data` |

If `opcode == 0x02`, `data` is an **RMI message** - see [../rmi/overview.md](../rmi/overview.md).

### 3.1 Offline path opcodes

| Opcode | Name | Direction | ENTRY | GAME |
| --- | --- | --- | --- | --- |
| `0x2F` | Hello | C2S | ✓ | ✓ |
| `0x04` | Key blob | S2C | ✓ | ✓ |
| `0x05` | Key response | C2S | ✓ | ✓ |
| `0x06` | ACK | S2C | ✓ | ✓ |
| `0x07` | Auth | C2S | ✓ | ✓ |
| `0x0A` | Redirect | S2C | ✓ | ✓ |
| `0x02` | Server info / RMI carrier | S2C | ✓ | ✓ |
| `0x1B` / `0x1D` | Ping / pong | C2S / S2C | - | ✓ |
| `0x1C` | Keepalive | both | C2S silent; S2C periodic | echo + periodic |
| `0x25` | Session | C2S | ✓ | ✓ (triggers lobby bursts) |

---

## 4. End-to-end sequences

### 4.1 ENTRY leg (port 7000)

| Step | C2S | S2C | Notes |
| --- | --- | --- | --- |
| E1 | `0x2F` + 4-byte tail | - | Tail: `0F 00 00 40`. |
| E2 | - | `0x04` + key blob | Static blob (§ opaque). |
| E3 | `0x05` + ciphertext | - | Opaque. |
| E4 | - | `0x06` ACK | |
| E5 | `0x07` + auth | - | Opaque. |
| E6 | - | `0x0A` redirect | IP only (§5.1). |
| E7 | `0x1C` keepalive | - | No S2C reply on ENTRY. |
| E8 | `0x25` + session blob | - | Opaque. |
| E9 | - | Two `0x02` server-info frames | §5.2. |
| E10 | - | S2C `0x1C` every 3 s | Until disconnect. |

After E9: client opens PROBE `:20009` and GAME `:27380`.

### 4.2 GAME leg (port 27380)

| Step | C2S | S2C | Notes |
| --- | --- | --- | --- |
| G1–G6 | Same as E1–E6 | Same as E2–E6 | Independent TCP connection. |
| G7 | `0x1C` | `0x1C` | Server echoes keepalive. |
| G8 | `0x1B` (opt) | `0x1D` pong | Pong = ping prefix + uint32 ms LE. |
| G9–G11 | `0x25` ×3 | Bursts 0–2 | Verbatim replay; see [../rmi/server.md](../rmi/server.md). |
| G12 | `0x25` (later) | - | No reply after bursts exhausted. |
| G13 | - | Post-init schedule (9 frames) | Delayed after last burst. |
| G14 | - | S2C `0x1C` every 5 s | |
| G15 | - | `0x02` / RMI `0xD2` every 10 s | If in post-init capture. |

### 4.3 PROBE (port 20009)

| Condition | Behavior |
| --- | --- |
| First byte `0x2F` | Echo entire chunk unchanged. |
| Otherwise | Ignored. No Proud framing. |

### 4.4 ctl stages (outcome, not wire)

| Stage | Meaning |
| --- | --- |
| `connecting_to_server` | `TCPSocket::Connect` toward port 7000. |
| `shard_choice` | `CGameServer::onPreProcess` begin. |
| `main_menu` / `server_ready` | Same hook, end - main menu wired. |

Requires GAME-leg replay success, not merely ENTRY handshake.

---

## 5. Partially understood server-built messages

### 5.1 Redirect (`opcode 0x0A`)

Built by `build_redirect_frame()`.

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | `conn_id` uint32 LE |
| 4 | 16 | `session_token` random |
| 20 | 4 | `flags` fixed `01 00 01 01` |
| 24 | 1 | `ip_len` |
| 25 | `ip_len` | ASCII host |
| 25+len | 2 | random trailer |

**No port field.**

### 5.2 ENTRY server-info (two 80-byte Proud frames)

```text
[ prefix_28 ][ server_name_36_UTF16LE ][ postfix_16 ]
```

| Piece | Source |
| --- | --- |
| Prefix/postfix | Capture constants `_ENTRY_INFO_*` in `proud_rmi.py` |
| `server_name` | `config.SERVER_NAME` (max 17 UTF-16 code units, padded to 36 B) |

Port-like bytes in prefix: `f4 6a` → **0x6AF4 = 27380**.

### 5.3 Pong (`opcode 0x1D`)

```text
body = ping_data || uint32_le(floor(time_ms))
```

---

## 6. Client requirements (offline)

1. **`TheGame.server_ip`** or **`THEGAME_SERVER_IP`** - remap production IPs on ports 7000, 27380, 20009.
2. All **`connect` IAT** hooks + ProudNet **`w_connect_2` / `w_connect_3`** - see [implementation.md](implementation.md).
3. **`pn_select_sync`** must not overwrite GAME socket with ENTRY socket after `:27380` connect.
4. Dummy server on all three ports (`just ensure-serve`).

---

## 7. Server configuration

| Variable | Default | Effect |
| --- | --- | --- |
| `PORT` | `7000` | ENTRY |
| `GAME_PORT` | `27380` | GAME |
| `PROBE_PORT` | `20009` | PROBE |
| `REDIRECT_HOST` | `127.0.0.1` | Redirect IP |
| `SERVER_NAME` | `TOM LOCAL EMU` | ENTRY server-info name field |
| `VERBOSE_WIRE_LOG` | `true` | `ctl/logs/server/wire_*.log` |

RMI trigger env vars: [../rmi/server.md](../rmi/server.md).

---

## 8. Quick reference

```text
Magic:           13 57 (0x5713 LE)
ENTRY:           7000  - transport FSM → server-info → GAME
GAME:            27380 - transport FSM → 3× replay burst on 0x25 → post-init
PROBE:           20009 - raw 0x2f echo
Hello (C2S):     2f 0f 00 00 40 (inside len=5 payload)
Key blob (S2C):  04 + [182 B PN_KEY_BLOB]
ACK (S2C):       06
Redirect (S2C):  0a + connId + token16 + flags4 + iplen + ip + rand2
RMI carrier:     02 [rmi_id:2 LE] [body…]
```

*Last updated: 2026-05-29 - transport layer; RMI bodies moved to `docs/rmi/`.*
