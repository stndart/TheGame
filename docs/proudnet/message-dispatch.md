# ProudNet - `MessageType` dispatch (data)

IDA **GAME** @ port **13337** (`GAME.exe`, base `0x400000`). Enum labels from [../proudnet-sdk-crossmap.md §6](../proudnet-sdk-crossmap.md). **Do not** confuse `MessageType` ordinals with transport opcodes (e.g. wire `0x2F`).

Hook status: [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md). Enum in code: [`include/game/net/pn_message_type.hpp`](../../include/game/net/pn_message_type.hpp).

---

## Core dispatch RVAs

| RVA | Symbol | Role |
| --- | --- | --- |
| **`0xD653B0`** | `sub_D653B0` | **`ProcessMessage_ProudNetLayer`** - `Message_Read` → 50-case switch; drained by `sub_D65940` |
| **`0xD366A0`** | `sub_D366A0` | Alternate dispatch - 48-case switch; caller `sub_D37BC0` (RMI unsafe batch); **not** client worker layer |
| **`0xD59300`** | `sub_D59300` | **`Message_Read(MessageType)`** |
| **`0xD58B30`** | `sub_D58B30` | **`CMessage::Read`** |
| **`0xD65940`** | `sub_D65940` | Receive-queue drain → `sub_D653B0` per message |

**v1.8 analogue:** `CNetClientWorker::ProcessMessage_ProudNetLayer` @ `0x1005A370` (PN18).

Compress / encrypt on **client** (`sub_D653B0`):

| Ordinals | Callee | v1.8 analogue |
| --- | --- | --- |
| **37, 38** | `0xD5DC10` → recurse layer | `ProcessMessage_Compressed` (PDB **47**) |
| **39** | `0xD5CA30` → recurse layer | `ProcessMessage_Encrypted` (PDB **43–46**) |

**Early returns:** `Message_Read` fails → restore offset (`0xD589C0`) → `return 0`; unhandled switch → same; success → `return 1`.

---

## `sub_D653B0` - full dispatch table

`IsFromRemoteClientPeer` = peer guard where noted. **`noop`** = `messageProcessed = 1`, no handler. **`default`** = empty arm.

| Case | v1.8 name | Handler RVA | Notes |
| --- | --- | --- | --- |
| 1 | `MessageType_Rmi` | `0xD64F10` | → game RMI layer |
| 2 | `MessageType_UserMessage` | `0xD65170` | |
| 3 | `MessageType_Hla` | `0xD5FFD0` | + peer guard |
| 4 | `MessageType_ConnectServerTimedout` | `0xD62930` | + peer guard |
| 5 | `MessageType_NotifyStartupEnvironment` | - | **default** |
| 6 | - | `0xD62FD0` | + peer guard |
| 7 | - | - | **default** |
| 8 | - | `0xD60020` | + peer guard |
| 9 | - | `0xD60070` | + peer guard |
| 10 | - | `0xD64760` | + peer guard |
| 11 | `MessageType_NotifyProtocolVersionMismatch` | `0xD5FF60` | + peer guard |
| 12 | `MessageType_NotifyServerDeniedConnection` | - | **default** |
| 13 | `MessageType_NotifyServerConnectSuccess` | `0xD61490` | + peer guard |
| 14 | - | - | **default** |
| 15 | `MessageType_NotifyAutoConnectionRecoverySuccess` | `0xD645C0` | + peer guard |
| 16 | `MessageType_NotifyAutoConnectionRecoveryFailed` | - | **default** |
| 17 | `MessageType_RequestStartServerHolepunch` | `0xD608D0` | + peer guard |
| 18 | - | - | **default** |
| 19 | `MessageType_ServerHolepunchAck` | `0xD63510` | |
| 20–23 | - | - | **default** |
| 24 | - | `0xD63750` | + peer guard |
| 25 | - | `0xD60A50` | + peer guard |
| 26 | - | `0xD63A70` | + peer guard |
| 27–28 | - | - | **default** |
| 29 | - | `0xD64D60` | + peer guard |
| 30, 48 | - | - | **noop** |
| 31 | - | `0xD5FCD0` | |
| 32 | - | `0xD5FD00` | |
| 33 | - | `0xD61F20` | |
| 34 | - | `0xD60E10` | |
| 35 | - | `0xD625E0` | |
| 36 | - | `0xD61760` | |
| 37–38 | Compressed | `0xD5DC10` | unwrap + recurse |
| 39 | Encrypted | `0xD5CA30` | unwrap + recurse |
| 40 | - | `0xD62110` | |
| 41 | - | `0xD60FB0` | |
| 42–47 | - | - | **default** on client |
| 49 | - | `0xD62330` | |
| 50 | - | `0xD61120` | |

Post-switch: unconsumed bytes (except types 37–38) → format error (`sub_D83110`).

---

## `sub_D366A0` - alternate path summary

Caller **`sub_D37BC0`**. Case **47** handled here only (client `sub_D653B0`: default). Cases **1–2** `return 0` after handler. Full table: [../proudnet-sdk-crossmap.md §6b](../proudnet-sdk-crossmap.md).

---

## Gaps vs v1.8 PDB (§6)

| §6 entry | GAME `sub_D653B0` |
| --- | --- |
| **1–4, 11, 13, 15, 17, 19** | Implemented |
| **5, 12, 16** | **default** |
| **21, 23** | **default** |
| **25–42** UDP/relay/ping | Partial; **27–28, 42–46 default** |
| **47** Compressed | Client uses **37–38**; **47 unhandled** |
| **43–46** encrypted | Client **39** only |
| **54–56** multicast/linger | **default** |

**Other `Message_Read` xrefs:** `0xD8B4A0` (`GetWorkTypeFromMessageHeader`), `0xD28660`.

*Last updated: 2026-05-29.*
