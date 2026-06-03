# Game RMI - client (data + implementation)

How the **client** sends C2S game RMIs and dispatches S2C responses. RVAs: [../rmi-ida-reimpl.md](../rmi-ida-reimpl.md).

---

## C2S send path

Two chokepoints (plain prologue `55 8B EC 83 E4 F8` - steal 6, resume +6):

| EA | Signature | Role | id source |
| --- | --- | --- | --- |
| **`sub_65AEA0`** | `__thiscall(proxy, msg, len, rmiId:int16)` | Proxy wrapper (16xxx) | **explicit** `rmiId` |
| **`sub_A0B290`** | `(proxy, msg, len)` | Floor transmit (15xxx) | `*(u16*)msg` |

Application frame: **`[rmi_id:2 LE][body...]`**; `len` includes the 2-byte id.

**Not game RMIs:** framework `sub_D5C5E0` (`IRmiProxy::RmiSend`) - heartbeat ids **1001/1006/1019** only.

### Capture hooks

[`src/RMI/GameSendHook.cpp`](../../src/RMI/GameSendHook.cpp):

| Hook | Target | Log prefix |
| --- | --- | --- |
| `hook_pn_rmi_send` | `sub_D5C5E0` | `[rmi] c2s frame ...` |
| `hook_pn_game_rmi_send` | `sub_65AEA0` | `[rmi] c2s proxy ...` |
| `hook_pn_rmi_floor` | `sub_A0B290` | `[rmi] c2s floor ...` |
| `ProcessMessageProudNetLayer` (case Rmi) | `sub_D653B0` | `[rmi] s2c ...` |

Logs **id + len** (framework: + remotes); no filtering. Read body at `msg` for hex dumps.

---

## S2C receive path

| EA | Role |
| --- | --- |
| **`sub_D65940`** | Receive-queue drain → `sub_D653B0` per message. SEH resume `0xD65947`. |
| **`sub_D653B0`** | `ProcessMessage_ProudNetLayer` - case **1 (Rmi)** → `sub_D64F10`. Reimpl: [`ProcessProudNetLayer.cpp`](../../src/ProudNet/ProcessProudNetLayer.cpp); hook: [`pn_process_message_hook.cpp`](../../src/hooks/net/pn_process_message_hook.cpp). |
| **`sub_D64F10`** | RMI handler - dispatcher virtual resolves id → leaf. |
| **`sub_D00FE0`** | Peer resolver; `hostid == 1` → server fast-path. |

`netcore = *(worker + 0x5C)`. RMI dispatcher @ `netcore+5964`.

---

## RES leaf calling convention

Every S2C leaf: `int __stdcall handler(MsgDelegateArg const* a1)`, `retn 4`.

```text
body   = *(u32*)(a1 + 8)
+0x00  u16   pad (ignored)
+0x02  u16   result (0 = success; nonzero → sub_675160 popup)
+0x04  ...     handler-specific fields
```

Verified leaves:

- `sub_4BA070` (`0x3F3E` connect RES)
- `sub_437160` (`0x3F30` create RES) → `RequestState(9)` on success

---

## `CReceivedMessage` layout (full-fidelity path)

For dispatch through `sub_D653B0` (ctor `sub_D12450`):

| Off | Field | Direct-buffer mode |
| --- | --- | --- |
| `0x00` | bit read offset | `0` |
| `0x08` | shared-buffer ptr | **`0`** |
| `0x0C` | direct data ptr | `&flatbuf` |
| `0x10` | length | `buflen` |
| `0x1C` | remote HostID | server (`1` fast-path) |

Buffer: `[MessageType=0x01][rmi_id:2 LE][body]`.

> S2C dispatch uses the full `CReceivedMessage` path above. Fake-server inject was removed — see [fake-server-hooks.md](fake-server-hooks.md).

---

## RMI registrars

Pattern: `mov reg, offset HANDLER` → delegate → binder → `[proxy+4]` register.

| Registrar | EA | Binder | Count | Band |
| --- | --- | --- | --- | --- |
| `CAccountPacket::Register` | `sub_4B8630` | `sub_556CB0` | ~55 | account/lobby/room |
| `CCommonPacket::Register` | `sub_421070` | `sub_554930` | ~43 | common/in-match |
| Room proxy | `sub_4366E0` | `sub_555520` | 3 | create/list/close |
| Floor/match | `sub_43A680` | `sub_5555E0` | 36 | start cluster |

Full id → leaf tables: [../rmi-ida-reimpl.md](../rmi-ida-reimpl.md).

### Room proxy cluster

| RES id | Leaf | Role |
| --- | --- | --- |
| `0x3F2F` | `sub_437390` | room-list RES |
| **`0x3F30`** | **`sub_437160`** | create RES → scene 9 |
| `0x3F31` | `sub_437240` | room close |

---

## Verified action → REQ map (condensed)

| Action | REQ proxy | REQ floor | len | RES id → leaf |
| --- | --- | --- | --- | --- |
| lobby enter | `0x3F40` | `0x3ACE` | 2 | `0x3F41` → `sub_4BA740` |
| global chat | - | `0x3AD2` | ~36 | echo RES |
| Quick Dive | `0x3EE4` | `0x3AAF` | 3 | ? |
| room list | `0x3F2F` | `0x3A9F` | 2 | `0x3F2F` → `sub_437390` |
| **create room** | **`0x3F30`** | **`0x3AA0`** | **98** | **`0x3F30`** → **`sub_437160`** |
| room enter | `0x3ED3` | `0x3AC0` | 6 | `0x3ED4` → `sub_4BB560` |
| ready | `0x3F2B` | `0x3AA8` | 3 | **unknown** — [actions.md](actions.md) |
| team/loadout | - | `0x3AC6` | 10246 | ? |
| **start match** | **`0x3F3D`** | **`0x3AA9`** | 2 | **`0x3F3D`** → **`sub_43D9B0`** |
| **leave room** | **`0x3F45`** | **`0x3AA3`** | 6 | **`0x3F45`** → **`sub_43D020`** |

Full table + body layouts: [actions.md](actions.md).

### Create-room REQ body (98 B)

| off | type | meaning |
| --- | --- | --- |
| +0 | u16 | floor id `15008` |
| +2 | WCHAR[~26] | room name |
| ~+0x36 | u8 | password flag |
| ~+0x38 | WCHAR[] | password |
| ~+0x42 | u32×6 | mode, channel, max players, map, ... |

Create RES only needs `result@+2 == 0`.

### Start RES body (`sub_43D9B0`)

| off | type | use |
| --- | --- | --- |
| +2 | u16 | result |
| +4 | WCHAR[] | room/map name |
| +0x26 | u16 | game mode |
| +0x28 | u32 | match handle → `flt_1C25E2C+0x210` |

---

## In-process injection (removed)

The in-process S2C inject harness was **removed in v1**. Archive: [fake-server-hooks.md](fake-server-hooks.md) (do not trust inject RES ids for open actions). Use the **friends server** on the wire + `[rmi] s2c` logs.

---

## Client struct headers

| Struct | Path |
| --- | --- |
| NetUserConnectRES | [`include/game/net/net_user_connect_res.hpp`](../../include/game/net/net_user_connect_res.hpp) |
| Proud message summary | [`include/game/net/proud_message.hpp`](../../include/game/net/proud_message.hpp) |

*Last updated: 2026-05-29.*
