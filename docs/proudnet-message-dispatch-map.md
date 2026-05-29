# ProudNet `MessageType` dispatch (GAME.exe)

IDA **GAME** @ port **13337** (`GAME.exe`, image base `0x400000`). Enum labels from [proudnet-sdk-crossmap.md §6](proudnet-sdk-crossmap.md) (v1.8 PDB). **Do not** confuse `MessageType` ordinals with transport opcodes (e.g. wire `0x2F`).

## Verdict

| RVA | Symbol (IDA) | Role |
|-----|----------------|------|
| **`0xD653B0`** | `sub_D653B0` | **`ProcessMessage_ProudNetLayer`** — `CNetClientWorker`, **`sub_D59300` (`Message_Read`) first**, **50-case** switch, drained by **`sub_D65940`** |
| **`0xD366A0`** | `sub_D366A0` | Alternate dispatch — **`int* this`**, **48-case** switch, same `Message_Read`; **not** the client worker layer (caller **`sub_D37BC0`** batch loop) |
| **`0xD59300`** | `sub_D59300` | **`Message_Read(MessageType)`** — bit-align + **`sub_D58B30`** read 1 byte |
| **`0xD58B30`** | `sub_D58B30` | **`CMessage::Read`** (bit offset / buffer bounds) |
| **`0xD65940`** | `sub_D65940` | Receive-queue drain → **`sub_D653B0`** per message |

**TheGame.dll hooks (2026-05-28):** layer **`0xD653B0`** = full reimpl; drain **`0xD65940`** = trace (SEH resume `0xD65947`); compress **`0xD5DC10`** / encrypt **`0xD5CA30`** = trace passthrough. Details: [proudnet-hook-status.md](proudnet-hook-status.md).

**v1.8 analogue:** `CNetClientWorker::ProcessMessage_ProudNetLayer` @ **`0x1005A370`** (PN18) — same pattern: `Message_Read` → large `switch`; encrypted **43–46** → `ProcessMessage_Encrypted` + recurse; **47** → `ProcessMessage_Compressed` + recurse.

**GAME `sub_D653B0` matches that pattern** (worker type, `Message_Read`, ~50 cases, compress/encrypt unwrap + recurse). **`sub_D366A0` does not** (different `this`, cases **1/2** `return 0` after handler, **case 47** handled here but **default on client** `sub_D653B0`).

Compress / encrypt on **client** (`sub_D653B0`):

| Ordinals | Callee | v1.8 analogue |
|----------|--------|----------------|
| **37, 38** | `sub_D5DC10` → recurse `sub_D653B0` | `ProcessMessage_Compressed` (PDB **47**) |
| **39** | `sub_D5CA30` → recurse `sub_D653B0` | `ProcessMessage_Encrypted` (PDB **43–46**, single case in GAME) |

**Early returns (`sub_D653B0`):** `Message_Read` fails → restore read offset (`sub_D589C0`) → **`return 0`**; switch not handled (`messageProcessed == 0`) → same; success → **`return 1`**.

---

## `sub_D653B0` — full dispatch table

`IsFromRemoteClientPeer` = peer guard where noted. **`noop`** = `messageProcessed = 1`, no handler. **`default`** = empty `default:` arm (same as noop).

| Case | v1.8 name (§6, when known) | Handler RVA | Notes |
|------|----------------------------|-------------|-------|
| 1 | `MessageType_Rmi` | `0xD64F10` | Sets `messageProcessed` from handler |
| 2 | `MessageType_UserMessage` | `0xD65170` | |
| 3 | `MessageType_Hla` | `0xD5FFD0` | + peer guard |
| 4 | `MessageType_ConnectServerTimedout` | `0xD62930` | `OutputDebugStringW`; + peer guard |
| 5 | `MessageType_NotifyStartupEnvironment` | — | **default** (unhandled) |
| 6 | *(no §6 label)* | `0xD62FD0` | + peer guard |
| 7 | — | — | **default** |
| 8 | — | `0xD60020` | + peer guard |
| 9 | — | `0xD60070` | + peer guard |
| 10 | — | `0xD64760` | + peer guard |
| 11 | `MessageType_NotifyProtocolVersionMismatch` | `0xD5FF60` | + peer guard |
| 12 | `MessageType_NotifyServerDeniedConnection` | — | **default** |
| 13 | `MessageType_NotifyServerConnectSuccess` | `0xD61490` | + peer guard |
| 14 | — | — | **default** |
| 15 | `MessageType_NotifyAutoConnectionRecoverySuccess` | `0xD645C0` | + peer guard |
| 16 | `MessageType_NotifyAutoConnectionRecoveryFailed` | — | **default** |
| 17 | `MessageType_RequestStartServerHolepunch` | `0xD608D0` | + peer guard |
| 18 | — | — | **default** |
| 19 | `MessageType_ServerHolepunchAck` | `0xD63510` | Also xrefs **`sub_D653B0`** (relay) |
| 20–23 | — | — | **default** |
| 24 | — | `0xD63750` | + peer guard |
| 25 | — | `0xD60A50` | + peer guard; xrefs **`sub_D653B0`** |
| 26 | — | `0xD63A70` | + peer guard; xrefs **`sub_D653B0`** |
| 27–28 | — | — | **default** |
| 29 | — | `0xD64D60` | + peer guard |
| 30, 48 | — | — | **noop** |
| 31 | — | `0xD5FCD0` | |
| 32 | — | `0xD5FD00` | |
| 33 | — | `0xD61F20` | |
| 34 | — | `0xD60E10` | |
| 35 | — | `0xD625E0` | xrefs **`sub_D653B0`** |
| 36 | — | `0xD61760` | xrefs **`sub_D653B0`** |
| 37–38 | Compressed (v1.8 **47**) | `0xD5DC10` | Unwrap + **recurse** `sub_D653B0` |
| 39 | Encrypted (v1.8 **43–46**) | `0xD5CA30` | Unwrap + **recurse** `sub_D653B0` |
| 40 | — | `0xD62110` | |
| 41 | — | `0xD60FB0` | |
| 42–47 | Encrypted/compressed (v1.8) | — | **default** on client |
| 49 | — | `0xD62330` | |
| 50 | — | `0xD61120` | |

Post-switch: if `messageProcessed` and unconsumed bytes (except types **37, 38**), enqueue format error (`sub_D83110` / `NC.PNL`-style path).

---

## `sub_D366A0` — summary (non–ProudNet-layer)

| Case | Handler | Early return |
|------|---------|----------------|
| 1 | `0xD351F0` | **`return 0`** after `sub_D589C0` |
| 2 | `0xD353A0` | **`return 0`** after `sub_D589C0` |
| 5 | `0xD2B230` | → common success path |
| 7 | `0xD33DE0` | |
| 12 | `0xD27780` (+ assert path `0xD32A80`) | |
| 14 | `0xD27F10` | |
| 16 | `0xD27A40` | |
| 18–23 | `0xD29BA0` … `0xD34E80` | |
| 27–28 | `0xD27D20`, `0xD1D6C0` | |
| 37–38 | `0xD5DC10` + recurse | Same compress helper as client |
| 39 | `0xD5CA30` + recurse | Same encrypt helper |
| 40–41 | `0xD282E0`, `0xD1D770` | |
| **47** | **`0xD297C0`** | **Handled here only** (client: **default**) |
| 48 | noop | |
| 3–4,6,8–11,13,15,17,19,24–36,42–46 | — | **default** → `sub_D589C0`, **`return 0`** |

Caller: **`sub_D37BC0`** — iterates RMI unsafe messages (`read offset == 0` assert), **`sub_D366A0`** each.

---

## Unique per-case handlers (`sub_D653B0` only)

`sub_D64F10` `sub_D65170` `sub_D5FFD0` `sub_D62930` `sub_D62FD0` `sub_D60020` `sub_D60070` `sub_D64760` `sub_D5FF60` `sub_D61490` `sub_D645C0` `sub_D608D0` `sub_D63510` `sub_D63750` `sub_D60A50` `sub_D63A70` `sub_D64D60` `sub_D5FCD0` `sub_D5FD00` `sub_D61F20` `sub_D60E10` `sub_D625E0` `sub_D61760` `sub_D5DC10` `sub_D5CA30` `sub_D62110` `sub_D60FB0` `sub_D62330` `sub_D61120` — shared: `sub_D59300` `sub_D58B30` `sub_D589C0` `IsFromRemoteClientPeer` `sub_D12450` `sub_D124D0` `sub_D653B0` (recurse) `sub_D83110` …

---

## Gaps vs crossmap §6

| §6 entry | GAME `sub_D653B0` |
|----------|-------------------|
| **1–4, 11, 13, 15, 17, 19** | Implemented (see table) |
| **5, 12, 16** | **default** (no handler) |
| **21, 23** | **default** (20–23 block) |
| **25–42** (UDP/relay/ping…) | Partial: many ordinals implemented (8–10, 24–26, 29, 31–36, 40–41, 49–50); **27–28, 42–46 default** |
| **47** `MessageType_Compressed` | Client uses **37–38**, not **47**; **47 unhandled** on client |
| **43–46** encrypted | Client **39** only; **43–46 default** |
| **54–56** multicast/linger | **default** (not in switch arms) |

**Other `Message_Read` xrefs:** `sub_D8B4A0` (`GetWorkTypeFromMessageHeader` — types 1/2/else), `sub_D28660`.

---

*IDA MCP GAME 13337 + PN18 `0x1005A370` decomp, 2026-05-28.*
