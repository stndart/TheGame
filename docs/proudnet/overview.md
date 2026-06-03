# ProudNet - overview (intent)

How ProudNet is structured in **The Game** offline client path and what each layer does. For wire layouts see [protocol.md](protocol.md); for game RMI see [../rmi/overview.md](../rmi/overview.md).

---

## Components

```text
                    ┌─────────────────────────────────────┐
                    │  GAME.exe + TheGame.dll (client)     │
                    │  • ProudNet worker + connection FSM  │
                    │  • MessageType dispatch (internal)   │
                    │  • Game RMI registrars + UI FSM      │
                    │  • Offline remap → 127.0.0.1         │
                    └──────────┬──────────────────────────┘
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
         ▼                     ▼                     ▼
   TCP :7000              TCP :27380           TCP :20009
   ENTRY leg              GAME leg              PROBE
   (transport FSM)        (transport FSM +      (raw echo)
                          lobby replay)
         │                     │
         └──────────┬──────────┘
                    ▼
         ┌──────────────────────────┐
         │  server/server/serve.py   │
         │  entry_transport.py       │
         │  game_transport.py        │
         └──────────────────────────┘
```

| Component | Role |
| --- | --- |
| **Proud TCP frame** | Length-prefixed messages on every Proud leg (`13 57` magic). Parser `sub_D84BB0`, framer `sub_D84970`. |
| **Transport opcode** | First byte of inner payload - hello, key exchange, redirect, session (`0x25`), plaintext RMI carrier (`0x02`). |
| **Connection FSM** | Per-TCP-connection worker states 0-5 (`PNConnectionNode`, driver `sub_D6F7B0`). Select, send, recv, append. |
| **`CFastSocket`** | Overlapped WSASend/WSARecv on worker socket @ `this+0x12C`. |
| **`ProcessMessage_ProudNetLayer`** | Internal envelope dispatch (`sub_D653B0`): `Message_Read` → 50-case `MessageType` switch. Case **1** = RMI → game layer. |
| **Game RMI** | Application request/response ids (`0x3F3E`, `0x3F30`, ...) registered at startup. See [../rmi/overview.md](../rmi/overview.md). |
| **ctl stages** | UI milestones from `TheGame.dll` hooks (`shard_select`, `lobby`, `room_list`, …) — observable outcome, not wire opcodes. See [stages.md](stages.md). |

---

## TCP legs (offline)

| Leg | Port | Purpose |
| --- | --- | --- |
| **ENTRY** | 7000 | Master-server handshake → redirect + server-info (advertises game port **27380**). |
| **GAME** | 27380 | Second handshake → session `0x25` bursts (lobby replay). |
| **PROBE** | 20009 | Optional non-Proud echo check. |

Production hosts remap to `127.0.0.1` via `include/game/server_override.hpp` (`THEGAME_SERVER_OVERRIDE` / `THEGAME_SERVER_IP`).

**Important:** Redirect (`0x0A`) carries **IP only** - the client learns port **27380** from ENTRY server-info frames (`0x02`), not from the redirect blob.

---

## Layer stack (bottom to top)

```text
[ TCP bytes ]
    └─ Proud TCP frame (magic + varint length + payload )
        └─ Transport opcode (0x2F hello, 0x04 key, ..., 0x02 RMI carrier, 0x25 session )
            └─ (when MessageType path) internal envelope byte from Message_Read
                └─ (case 1) Game RMI id + body
```

Three **different** byte namespaces - do not conflate:

| Namespace | Examples | Doc |
| --- | --- | --- |
| Transport opcode | `0x2F`, `0x02`, `0x25` | [protocol.md §4](protocol.md) |
| ProudNet `MessageType` ordinal | `1`=Rmi, `37/38`=Compressed | [message-dispatch.md](message-dispatch.md) |
| Game RMI id | `0x3F3E`, `0x3F30` | [../rmi/overview.md](../rmi/overview.md) |

---

## Key C++ layout / constants

| Artifact | Path |
| --- | --- |
| Offsets + `Proud::Rva::*` | [`include/ProudNet/Layout.hpp`](../../include/ProudNet/Layout.hpp) |
| Growable buffer | [`include/ProudNet/GrowableBuffer.hpp`](../../include/ProudNet/GrowableBuffer.hpp) |
| Fast socket | [`include/ProudNet/FastSocket.hpp`](../../include/ProudNet/FastSocket.hpp) |
| Connection FSM | [`include/ProudNet/ConnectionNode.hpp`](../../include/ProudNet/ConnectionNode.hpp) |
| Proud frame (client) | [`include/game/net/proud_frame.hpp`](../../include/game/net/proud_frame.hpp) |
| MessageType enum | [`include/ProudNet/MessageType.hpp`](../../include/ProudNet/MessageType.hpp) |

---

## Related docs

| Topic | Doc |
| --- | --- |
| Wire bytes, sequences | [protocol.md](protocol.md) |
| `MessageType` switch | [message-dispatch.md](message-dispatch.md) |
| Reimpl file map | [implementation.md](implementation.md) |
| ctl stages | [stages.md](stages.md) |
| RVA / hook index | [../proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md) |
| SDK ↔ GAME crossmap (historical) | [../proudnet-sdk-crossmap.md](../proudnet-sdk-crossmap.md) |
| Current hook matrix | [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md) |
| Folder index | [README.md](README.md) |

*Last updated: 2026-06-03.*
