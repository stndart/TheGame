# Game RMI - overview (intent)

Application-layer **request/response** messages above ProudNet transport. The game registers hundreds of S2C handlers at startup; the client sends C2S requests through proxy wrappers. This doc defines intent and numbering; wire bodies in [server.md](server.md), client paths in [client.md](client.md).

**Living investigation:** [../plans/proudnet-game-rmi.md](../plans/proudnet-game-rmi.md). **Emulation plan:** [../plans/proudnet-rmi-server-plan.md](../plans/proudnet-rmi-server-plan.md).

---

## Mental model

```text
            C2S (client → server)                       S2C (server → client)
 UI click ─▶ send-proxy (sub_65AEA0 / sub_A0B290)        wire 0x02 RMI frame
            └▶ app frame [id:2][body]                   └▶ drain sub_D65940
               └▶ transport wrap (0x25)                      └▶ dispatch sub_D653B0
                  └▶ TCP [13 57][len][0x25 ...]   (opaque)        └▶ MessageType case Rmi
                                                                   └▶ sub_D64F10 → leaf handler
```

**Key facts:**

- **C2S app RMIs are NOT plaintext on the socket** - wrapped in transport `0x25`. Capture in-process at the send proxy. **[V]**
- **S2C app RMIs ARE plaintext:** `[0x02][rmi_id:2 LE][body]`. **[V]**
- **REQ and RES often share the same id** (e.g. create-room `0x3F30` → `0x3F30`). Do not assume RES = REQ−1. **[V]**

---

## Three numbering spaces

| Space | Examples | Where |
| --- | --- | --- |
| **Transport opcode** | `0x02`, `0x25`, `0x2F` | First byte of Proud inner payload - [../proudnet/protocol.md](../proudnet/protocol.md) |
| **ProudNet MessageType** | `1`=Rmi, `37/38`=Compressed | `Message_Read` inside `sub_D653B0` - [../proudnet/message-dispatch.md](../proudnet/message-dispatch.md) |
| **Game RMI id** | `0x3F30`, `0x3AA0`, `0x3F3D` | 2-byte id in app frame / after `0x02` on wire |

### Dual id bands (same logical action)

| Band | Range | Send path |
| --- | --- | --- |
| **Proxy (16xxx)** | `0x3E__` / `0x3F__` | `sub_65AEA0` - explicit `rmiId` arg |
| **Floor (15xxx)** | `0x3A__` / `0x3B__` | `sub_A0B290` - `*(u16*)msg` |

`sub_65AEA0` tail-calls `sub_A0B290`, so one action often logs both ids.

| Global | Band | Role |
| --- | --- | --- |
| `dword_1C1ABA0` | 16xxx | account → lobby → room |
| `dword_1C1ABB0` | 15xxx | match / party / in-game |

---

## Wire RMI carrier (S2C)

When transport opcode is `0x02`:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 1 | `0x02` |
| 1 | 2 | `rmi_id` uint16 LE |
| 3 | n | `body` |

C2S uses the same `[id:2][body]` layout **before** transport wraps it into `0x25`.

---

## Connect-path RMI ids (lobby)

| RMI id | Name | Body size (offline) | Status |
| --- | --- | --- | --- |
| `0x3F3E` | NetUserConnectRES | **644 B** (replay) / 384 B min (IDA) | IDA-confirmed - [server.md § NetUserConnectRES](server.md#netuserconnectres-rmi-0x3f3e) |
| `0x3E8E` | NetConnectRES | 64 B stub | WIP |
| `0x3E99` | NOTIFY | 6 B stub | WIP |
| `0x00D2` | periodic tick | From capture | Opaque replay |

Lobby replay contains many additional ids - see [server.md](server.md).

---

## Game state machine (scenes)

Transition: **`sub_41F0D0(CStateMachine*, sceneId)`**. State manager @ `dword_1C155C0`.

| Scene | Class | ctl stage (approx) |
| --- | --- | --- |
| 4 | CGameLobby | `lobby` |
| 5 | CGameRoomList | `room_list` |
| 9 | CGameRoom | `room` |
| 11 | CGamePlay | `in_game` / map load |

Full stage → RVA table: [../plans/proudnet-rmi-server-plan.md](../plans/proudnet-rmi-server-plan.md).

---

## Related docs

| Topic | Doc |
| --- | --- |
| C2S send, S2C receive, registrars | [client.md](client.md) |
| Autonomous lobby → create room (ctl) | [autonav.md](autonav.md) |
| Wire builders, replay, triggers | [server.md](server.md) |
| RVA / handler tables | [../rmi-ida-reimpl.md](../rmi-ida-reimpl.md) |

*Last updated: 2026-05-29.*
