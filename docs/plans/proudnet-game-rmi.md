# Game RMI protocol & in-process emulation (GAME.exe)

**Status:** living reference, started 2026-05-29 (runs 179–181). This is the **application-layer (game RMI)** reference - how the game's own request/response messages are sent, received, dispatched, and how we drive the client forward by **injecting S2C responses in-process**. Companion to the transport docs ([../proudnet/protocol.md](../proudnet/protocol.md), [../proudnet/message-dispatch.md](../proudnet/message-dispatch.md), [../proudnet-sdk-crossmap.md](../proudnet-sdk-crossmap.md)) and the working plan ([proudnet-rmi-server-plan.md](proudnet-rmi-server-plan.md)). **Stateless specs:** [../rmi/overview.md](../rmi/overview.md), [../rmi/client.md](../rmi/client.md), [../rmi/server.md](../rmi/server.md).

All addresses are **IDA effective addresses == RVA** (image base `0x400000`), e.g. `sub_437160` lives at VA `0x437160`. GAME.exe IDA instance @ MCP port **13337**.

Confidence tags: **[V]** verified live (DLL hook + run logs) or by direct decompile; **[D]** decompiled but not exercised; **[S]** suspicion / inferred / needs confirmation.

---

## 0. TL;DR mental model

```
            C2S (client → server)                       S2C (server → client)
 UI click ─▶ send-proxy (sub_65AEA0 / sub_A0B290)        wire 0x02 RMI frame
            └▶ app RMI frame [id:2][body]                 └▶ drain sub_D65940
               └▶ transport wrap (0x25 reliable/compress)    └▶ dispatch sub_D653B0
                  └▶ TCP [13 57][len][0x25 …]   (opaque)        └▶ MessageType switch → case Rmi
                                                                   └▶ sub_D64F10 (read id, find leaf)
                                                                      └▶ RES leaf handler(MsgDelegateArg*)
                                                                         body = *(u32*)(arg+8)
                                                                         result u16 @ body+2 (0=ok)
                                                                         → drives CStateMachine scene
```

Key facts:

- **C2S app RMIs are NOT plaintext on the wire** - they are wrapped in transport opcode `0x25` (reliable/compressed), so you cannot read `[id][body]` directly off the socket. Capture C2S **in-process** at the send proxy. **[V]**
- **S2C app RMIs ARE plaintext**: wire payload `[0x02][rmi_id:2 LE][body]`. `NetUserConnectRES 0x3F3E` is built exactly this way and works. **[V]**
- **REQ and RES often share the same id** (round-trip), e.g. create-room REQ `0x3F30` → RES `0x3F30`; start REQ `0x3F3D` → RES `0x3F3D`. The old "RES = REQ+1" rule is **not reliable** - look the RES up in the registrar tables. **[V]**

---

## 1. Wire framing recap (transport layer)

(See [../proudnet/protocol.md](../proudnet/protocol.md) for the full handshake. Repeated here only as it pertains to RMI.)

Outer TCP frame (framer recv `sub_D84BB0`, send `sub_D84970`):

```
[13 57] [len_tag] [len_bytes…] [payload]
  • magic 0x5713 (bytes 13 57)
    • len_tag: 1 → 1-byte len, 2 → 2-byte LE, 4 → 4-byte LE  (sub_D59250 / sub_D59DB0)
```

`payload[0]` = **transport opcode**:

| op | meaning | RMI relevance |
|----|---------|---------------|
| `0x2F` | hello | handshake |
| `0x04`/`0x05`/`0x06` | key blob / key resp / ack | handshake |
| `0x07`/`0x0A` | auth / redirect | handshake |
| `0x1C` | keepalive | - |
| `0x1B`/`0x1D` | ping / pong | - |
| **`0x02`** | **plaintext RMI message** | **`[0x02][rmi_id:2 LE][body]`** - used for S2C RES **[V]** |
| **`0x25`** | reliable/compressed session payload | **wraps C2S app RMIs** (opaque without decompress/decrypt) **[V]** |

> For the wire-capture effort: a C2S app RMI is serialized as `[rmi_id:2][body]` (see §3) *before* transport wraps it into a `0x25` frame. To read it off the wire you must undo the `0x25` reliable/compress layer (`ProcessMessage_Compressed` `sub_D5DC10`, `ProcessMessage_Encrypted` `sub_D5CA30`). The cheaper route used by this project is the in-process send hook (§3) which sees `id`+`len` pre-wrap.

---

## 2. Three numbering spaces - do not conflate

| Space | Examples | Where it appears |
|-------|----------|------------------|
| **Transport opcode** | `0x02`, `0x25`, `0x2F` | `payload[0]` on the wire |
| **ProudNet MessageType** (ordinal) | `1`=Rmi, `2`=UserMessage, `37/38`=Compressed, `39`=Encrypted | first byte read by `Message_Read sub_D59300` inside `sub_D653B0`; see [../proudnet/message-dispatch.md](../proudnet/message-dispatch.md) |
| **Game RMI id** | `0x3F30`, `0x3AA0`, `0x3F3D` | the 2-byte id after the MessageType, looked up against the registrar tables (§7) |

The RMI id has **two banded encodings for the same logical action** (see §3): a **16xxx proxy id** (`0x3E__/0x3F__`) and a **15xxx floor id** (`0x3A__/0x3B__`).

---

## 3. C2S send path - two chokepoints, two proxies

Two send functions, both with the plain prologue `55 8B EC 83 E4 F8` (steal 6, resume +6):

| EA | Signature | Role | id source |
|----|-----------|------|-----------|
| **`sub_65AEA0`** | `__thiscall(this=proxy, msg, len, rmiId:int16)` | proxy wrapper; account/lobby/room band | **explicit** `rmiId` arg (16xxx) |
| **`sub_A0B290`** | `(proxy, msg, len)` (`a1=eax`/userpurge) | floor transmit; match/party band, also inline senders that bypass the wrapper | `*(u16*)msg` (15xxx/18xxx) |

`sub_65AEA0` **tail-calls** `sub_A0B290`, so one logical send appears at both layers: a 16xxx proxy line immediately followed by a 15xxx floor line of the same `len`. A floor line with **no** matching proxy is a standalone inline sender. **[V]**

Two proxy singletons:

| Global | Band | Sent via | Carries |
|--------|------|----------|---------|
| **`dword_1C1ABA0`** | 16xxx (`0x3E__`/`0x3F__`) | `sub_65AEA0` | account → lobby → room → shop |
| **`dword_1C1ABB0`** | 15xxx (`0x3A__`/`0x3B__`) | `sub_A0B290` direct | match / party / in-game |

The application RMI frame handed to the floor is **`[rmi_id:2 LE][body…]`** (`*(u16*)msg` == id). `len` includes the 2-byte id. **[V]**

**Not the game RMIs:** framework proxy core `sub_D5C5E0` (`IRmiProxy::RmiSend`, SEH resume `0xD5C5E7`) carries only builtin heartbeat/session ids **1001/1006/1019** (`0x3E9/0x3EE/0x3FB`). Ignore for gameplay. **[V]**

**Our capture hooks** ([`src/RMI/GameSendHook.cpp`](../src/RMI/GameSendHook.cpp)): `hook_pn_game_rmi_send` @ `sub_65AEA0` logs `c2s_grmi proxy id=… len=…`; `hook_pn_rmi_floor` @ `sub_A0B290` logs `c2s_grmi floor id=… len=…`. They currently log **id + len only** - to dump bodies for wire correlation, read `len` bytes at `msg` (`[esp+0x28]` in the floor stub after pushad/pushfd) and hexdump.

---

## 4. S2C receive path - drain → dispatch → leaf

| EA | Role |
|----|------|
| **`sub_D65940`** | `CNetClientImpl::OnMessageReceived` - receive-queue drain; per message → `sub_D653B0`. ECX = worker. SEH prologue, resume `0xD65947`. Runs on the receive/dispatch thread (tid ≠ main; see §10). **[V]** |
| **`sub_D653B0`** | `ProcessMessage_ProudNetLayer` `__thiscall(worker, wstr_body, received_msg)` - `Message_Read` 1-byte type → 50-case switch. **case 1 (Rmi) → `sub_D64F10`.** Full reimpl in [`src/game/net/pn_process_message.cpp`](../src/game/net/pn_process_message.cpp). **[V]** |
| **`sub_D59300`** | `Message_Read(MessageType)` - bit-align then read 1 byte. |
| **`sub_D58B30`** | `CMessage::Read` core (bit offset / bounds / memcpy). Byte-aligned reads. |
| **`sub_D589C0`** | restore read offset (used on early-return). |
| **`sub_D64F10`** | RMI handler: `v4 = received>>3` (save offset); `peer = sub_D00FE0(*(received+0x1C))` (resolve sender from HostID); call registered dispatcher virtual `(*(netcore+5964 +8))(netcore+5964, received, peer)`; fallback `netcore+5924`; else builds "unhandled" frame. **The 2-byte id read + leaf lookup happen inside the dispatcher virtual**, which wraps the body into a `MsgDelegateArg` and calls the leaf. **[D]** |
| **`sub_D00FE0`** | peer resolver. `hostid == 1` → server peer at `netcore+0x14DC` (fast path). **[D/S]** |

`netcore = *(worker + 0x5C)`. RMI dispatcher object at `netcore+5964` (`0x174C`), secondary at `netcore+5924` (`0x1724`); netcore lock at `+0xB4`. **[D]**

---

## 5. RES leaf calling convention (the important one) **[V]**

Every registered S2C leaf is bound as a delegate `MSG_RETURN handler(MsgDelegateArg const* a1)` - `int __stdcall(a1)`, `retn 4`.

```
body   = *(u32*)(a1 + 8)        // raw RMI body pointer
+0x00  u16   pad / seq / echoed id   (IGNORED by handlers)
+0x02  u16   result / error code     (0 = success; nonzero → sub_675160 popup, no transition)
+0x04  …     handler-specific fields
```

Verified against two independent leaves:

- `sub_4BA070` (connect RES `0x3F3E`): `v2 = *(a1+8); if (*(u16*)(v2+2)) { sub_675160(...); return; } sub_8DA5D0(v2); …` - parses the 0x180 body from `v2`. **[V]**
- `sub_437160` (create RES `0x3F30`): `if (*(u16*)(*(a1+8)+2)) sub_675160(...); else … sub_41F0D0(9);` **[V]**

**`sub_675160(u16 code)`** = shared "server error" popup. Seeing it = your result field was nonzero.

This convention is what makes **direct leaf invocation** possible (§10): build a flat `body` buffer, put its pointer at `arg+8`, call the leaf. The handler only reads `*(a1+8)` for the body; no other `MsgDelegateArg` field is touched on the paths exercised so far.

---

## 6. `CReceivedMessage` layout (for full-fidelity injection) **[D]**

If you instead want to go through the real dispatcher (`sub_D653B0`) you must build a `CReceivedMessage` (ctor `sub_D12450`, ~`0x30` bytes):

| Off | Field | Set to (direct-buffer mode) |
|-----|-------|------------------------------|
| `0x00` | bit read offset (`>>3`=byte; must stay 8-aligned) | `0` |
| `0x08` | shared-buffer obj ptr | **`0`** (⇒ direct mode; dtor frees nothing) |
| `0x0C` | direct data ptr | `&flatbuf` |
| `0x10` | length (bytes) | `buflen` |
| `0x1C` | remote HostID (sender) | server HostID (`1` fast-path **[S]** - verify by logging `+0x1C` on a real S2C) |
| `0x20` | `&Proud::AddrPort::vftable` | set by ctor |
| `0x24/0x28` | AddrPort addr/port | ctor defaults |

Buffer for the dispatcher path = `[MessageType=0x01][rmi_id:2 LE][body]`, offset 0. `sub_D12170` resolves data ptr (direct `+0x0C` else shared `*(+0x08)+8`). Reads are byte-aligned, so a flat LE buffer works. **Length must equal bytes consumed** or `sub_D653B0` logs a consume-mismatch error.

> In practice we use the simpler **direct leaf call** (§5/§10), not this path, but the layout is documented for the in-game/per-frame sync messages whose leaves may read more `MsgDelegateArg` fields than `+8`.

---

## 7. RMI registrars & handler tables **[D]**

Four registrars bind S2C leaves. Pattern: `mov reg, offset HANDLER` → build delegate → `call <binder>` → set id immediate → `call [proxy+4]`.

| Registrar | Binder | Count | Band |
|-----------|--------|-------|------|
| `CAccountPacket::Register` **`sub_4B8630`** | `sub_556CB0` | ~55 | account/lobby/room `0x3E__/0x3F__` |
| `CCommonPacket::Register` **`sub_421070`** | `sub_554930` | ~43 | common/in-match `0x36__/0x37__/0x3F__` |
| `CGameRoomList` room proxy **`sub_4366E0`** | `sub_555520` | 3 | room create/list/close |
| floor/match **`sub_43A680`** | `sub_5555E0` | 36 | match/play `0x3F__` start cluster |

### 7a. Room proxy `sub_4366E0` (the create/list cluster) **[V]**

| RES id | leaf | role |
|--------|------|------|
| `0x3F2F` | `sub_437390` | room-**list** RES (parses N×144-byte room records) |
| **`0x3F30`** | **`sub_437160`** | **create / enter-room RES** → on `result==0`, `RequestState(9)` |
| `0x3F31` | `sub_437240` | room close / leave RES (`RequestState(-1)`) |

### 7b. Floor/match `sub_43A680` (start cluster) **[D]**

Most relevant: **`0x3F3D` → `sub_43D9B0`** = **game-start RES** (→ scene 11). Others seen: `0x3F32→43CCC0`, `0x3F45→43D020`, `0x3F46→43CE00`, `0x3F2B..0x3F2E`, `0x3F29/0x3F2A` (room member/state), `0x3F56→440020`.

### 7c. `CAccountPacket::Register sub_4B8630` (full table) **[D]**

| id | leaf | id | leaf | id | leaf |
|----|------|----|------|----|------|
| 3f3e | 4ba070 | 3ed8 | 4bb2e0 | 3ef8 | 4bb6d0 |
| 3e8e | 4ba520 | 3edc | 4bb370 | 3ef9 | 4bb6f0 |
| 3e91 | 4ba540 | 3ede | 4bb390 | 3f12 | 4bb710 |
| 3e92 | 4ba650 | 3ee1 | 4bb3f0 | 3f10 | 4bb7d0 |
| 3e93 | 4ba670 | **3f81** | **4bb450** | **3f11** | **4bb7e0** |
| 3f41 | 4ba740 | 3ed4 | 4bb560 | 3f20 | 4bb9f0 |
| 3f42 | 4ba790 | 3f6c | 4bb580 | 3f21 | 4bba10 |
| 3e95 | 4ba920 | 3f6d | 4bb5a0 | 3f22 | 4bba30 |
| 3f13 | 4baa10 | 3ed7 | 4bb5c0 | 3f24 | 4bba50 |
| 36e3 | 4baad0 | 3efc | 4bb5e0 | 3f25 | 4bba70 |
| 3ebe | 4bac90 | 3efd | 4bb620 | 3f26 | 4bba80 |
| 3ebf | 4bacd0 | 3f00 | 4bb740 | 3f27 | 4bbac0 |
| 3eca | 4bae10 | **3f03** | **4bb8f0** | 3f1f | 4bbae0 |
| **3f47** | **4baeb0** | 3f01 | 4bb7a0 | 3f4e | 4bbb90 |
| 3ea4 | 4bafc0 | 3ef6 | 4bb670 | 3f4f | 4bbbb0 |
| 3f50 | 4bbc30 | 3f52 | 4bbc80 | 3f53 | 4bbcd0 |
| 3f54 | 4bbd30 | 3f55 | 4bbd70 | 3f57 | 4bbdc0 |
| 3f58 | 4bbe00 | 3f59 | 4bbde0 | 3f6a | 4bbe20 |

### 7d. `CCommonPacket::Register sub_421070` (full table) **[D]**

| id | leaf | id | leaf | id | leaf |
|----|------|----|------|----|------|
| 36b4 | 423fc0 | 36c0 | 425b70 | 3f5e | 4273a0 |
| 36c6 | 423ff0 | 36b7 | 4256c0 | 3f5f | 427400 |
| 36df | 424050 | 3edf | 425f90 | 3f60 | 4279e0 |
| 36e0 | 4240b0 | 3f18 | 426000 | 3f61 | 427b80 |
| 36fc | 427090 | 3f1a | 426040 | 3f62 | 427bc0 |
| 36fd | 4270a0 | 3f1b | 426100 | 3f63 | 427ae0 |
| 370b | 424280 | 3f1c | 426110 | 3f64 | 427c00 |
| 370c | 424380 | 3f39 | 426250 | 3f65 | 427b30 |
| 3f44 | 424c80 | 36b2 | 423960 | 371a | 427c80 |
| 36b5 | 4250a0 | 3ed2 | 426fc0 | 371b | 427cc0 |
| 3e85 | 425080 | 3f17 | 427050 | 3f66 | 427a80 |
| 36b6 | 425160 | 3f5c | 4272c0 | 3f67 | 427c50 |
| 36c7 | 425720 | 3f5d | 427330 | 3717 | 4275c0 |
| 36b8 | 425730 | | | 3718 | 4275d0 |
| | | | | 3719 | 427930 |
| | | | | 3f6f | 427db0 |

---

## 8. Game state machine (scenes) **[V/D]**

State manager base global **`dword_1C155C0`** (`CStateMachine`):

| Field | Off | Global alias |
|-------|-----|--------------|
| active state ptr | +128 | `dword_1C15640` |
| current scene id | +132 | `dword_1C15644` |
| requested next scene | +140 | `dword_1C1564C` |

Transition request = **`sub_41F0D0(this=&dword_1C155C0, sceneId)`** (`CStateMachine::RequestState`). **[V]**

Scene→class factory = `sub_41A920`.

| Scene | Class | onPreProcess | Notes |
|-------|-------|--------------|-------|
| 4 | CGameLobby | 0x42BD50 | main menu (chat + Quick/Custom) |
| **5** | CGameRoomList | 0x4362E0 | custom-match list + Create |
| 6 | CGameMatchPartyRoom | 0x42F690 | |
| **9** | **CGameRoom** | 0x439B00 (ctor 0x439900) | waiting room (Ready) - **reached via create RES** **[V]** |
| 11 | CGamePlay | 0x47F610 | in-match; runs CBasePlayLoading map load (`0x4806E0`, via `sub_480680`); map id `dword_1C1A980`, begin-load `sub_6D9C70`; **gated on connect to game server** **[S]** |
| 14 | CGameIntro | 0x42A010 | |
| 20 | CGameResult | - | |
| 21 | (in-play variant?) | - | checked by `sub_6653B0` before sending start **[S]** |

Other stages (intro/login/shard) - see [proudnet-rmi-server-plan.md](proudnet-rmi-server-plan.md) §"Game-state machine".

---

## 9. Verified action → REQ / RES map (runs 179–181) **[V]**

C2S captured in-process; RES leaves confirmed by decompile. "proxy" = 16xxx via `sub_65AEA0`/`1C1ABA0`; "floor" = 15xxx via `sub_A0B290`/`1C1ABB0`.

| Action | REQ proxy | REQ floor | len | RES id → leaf | Effect of RES |
|--------|-----------|-----------|-----|---------------|---------------|
| connect (NetUserConnect) | - | - | - | `0x3F3E` → `sub_4BA070` | **644 B** offline capture (`lobby_replay` burst 1); documented min 0x180 (see `LOBBY_CONNECT_3F3E_BODY`) |
| lobby enter | `0x3F40` (16192) | `0x3ACE` (15054) | 2 | `0x3F41` → `sub_4BA740` **[S]** | |
| global chat send | - | `0x3AD2` (15058) | ~36 (text) | echo RES **[S]** | renders message |
| Quick Dive (lobby) | `0x3EE4` (16100) | `0x3AAF` (15023) | 3 | ? (automatch) | |
| room-list request | `0x3F2F` (16175) | `0x3A9F` (15007) | 2 | `0x3F2F` → `sub_437390` | fills room list |
| **create room** | **`0x3F30`** (16176) | `0x3AA0` (15008) | **98** | **`0x3F30`** → **`sub_437160`** | `result==0` → `RequestState(9)` (→ room) **[V]** |
| room enter (auto on entering room) | `0x3ED3` (16083) | `0x3AC0` (15040) | 6 | `0x3ED4` → `sub_4BB560` | room-enter ack **[S]** |
| **Ready toggle** | **`0x3F2B`** (16171) | `0x3AA8` (15016) | 3 | `0x3F2C`? **[S]** | marks ready in UI |
| team change | - | `0x3AC6` (15046) | **10246** | ? | uploads player/loadout block |
| **start match** (networked) | **`0x3F3D`** (16189) | `0x3AA9` (15017) | 2 | **`0x3F3D`** → **`sub_43D9B0`** | `result==0` → set params → `RequestState(11)` **[V/D]** |
| start (alt, `sub_665940`) | `0x3F3A` (16186) | (15014) | 2 | ? | |
| **leave room** (`sub_665400`) | **`0x3F45`** (16197) | **`0x3AA3`** (15011) | **6** | **`0x3F45`** → **`sub_43D020`** | mode `dword_1C1A87C` → `RequestState(4/5/6)`; body unused in decompile **[D/WIP]** |
| server-screen enter/leave | `0x3F0C` (16140) | `0x3AD1`/`0x3ACD` | 2 | | |

### 9a. Create-room REQ body (`sub_48A7C0` "ClickCreateRoom", 98 B) **[D]**

| off | type | meaning |
|-----|------|---------|
| +0 | u16 | floor id `15008` |
| +2 | WCHAR[~26] | room name (UTF-16; validated ≥2 chars, profanity `sub_9716D0`) |
| ~+0x36 | u8 | password-present flag |
| ~+0x38 | WCHAR[] | password |
| ~+0x42 | u32×6 | mode, channel (`sub_48C5F0`), max players, map id, option, … |
| late | u8 | friendly-fire/observer flag |

The **create RES does not echo these** - `sub_437160` only needs `result@+2 == 0`; the room config was built locally during the REQ. **[V]**

### 9b. Start RES body (`sub_43D9B0`, id `0x3F3D`) - exact, from asm **[D]**

Success path `loc_43DA92` (when `*(u16)(body+2) == 0`):

| off | type | use |
|-----|------|-----|
| +2 | u16 | result (0 = success) |
| +4 | WCHAR[] | room/map name; `sub_65C1C0(this=body+4, mode=*(u16)(body+0x26))` copies into `dword_1C1D960` block, sets `word_1C1D982 = mode` |
| +0x26 | u16 | game mode |
| +0x28 | u32 | → `flt_1C25E2C[0x210]` (match/server handle? map?) **[S]** |

### 9c. Start REQ sender (`sub_6653B0`) **[D]**

```c
bool sub_6653B0(this) {
  if (dword_1C15640 && (active_state->vtbl+4)(dword_1C15640, this) == 21) return 1;
  word = 15017;
  return sub_65AEA0(&dword_1C1ABA0, &word, 2, 16189) != 0;   // proxy 0x3F3D
}
```

Called from `ClickGameStart` UI `sub_43C0D0` (cases 2–11). Case 0 = solo/no-net: **directly** `RequestState(11)` with no network. Case 1 → `sub_665940` (proxy `0x3F3A`/16186, floor 15014).

---

## 10. Wire S2C builders (Python server) **[V/WIP]**

**Format (same as replay):** one GAME-leg **burst** = concatenated Proud TCP frames, each inner payload `[0x02][rmi_id:2 LE][body…]`. See `server/server/proud_rmi.py`, `proud_frame.py`, `game_transport.py`.

| RMI | Builder | Min body (IDA) | Notes |
|-----|---------|----------------|-------|
| `0x3F30` | `build_create_room_res_frame()` | 4 B, `result@+2==0` | `sub_437160` → scene 9 |
| `0x3ED4` | `build_room_enter_res_frame()` | 6+32×N | compact member map |
| `0x3ED8` | `build_room_members_ui_res_frame()` | 6+168×N | UI map (`sub_4BB370`) |
| `0x3F3E` | `build_net_user_connect_res_frame()` | **644 B** offline (`LOBBY_CONNECT_3F3E_BODY`, burst 1) / 384 B min (IDA) / 640 B (2017 FA) | connect path; name patch @ +0x10 |
| `0x3F3D` | `build_start_match_res_frame()` | **0x40 B** (`result@+2==0`) | `sub_43D9B0` → scene 11 |
| `0x3F45` | `build_leave_room_res_frame()` | **4 B** (minimal) **[WIP]** | `sub_43D020`; handler ignores `body` - transitions from `dword_1C1A87C` only **[D]** |

**Leave-room RES (`sub_43D020`, id `0x3F45`) - [WIP]:** Registrar `sub_43A680` @ `0x43AE97`. REQ sender `sub_665400` (`v2=15011`, `len=6`, proxy `16197`). Decompile does **not** read `*(a1+8)` or `result@+2`; success path calls `RequestState(5)` (room list) when `dword_1C1A87C==0`. Wire builder uses 4 B `result=0` like create-room. **Trigger:** `GENERATE_LEAVE_ROOM_RES_ON_REQ` + `LEAVE_ROOM_RES_ON_25_BODY_LEN=19` (run 187 `game_proudnet_tcp.txt`); **collides** with room-enter (`0x3ED3`) and Ready (`0x3F2B`) on the same opaque 19 B blob - prefer `LEAVE_ROOM_RES_AFTER_CREATE_NTH_25` or in-process inject when both are enabled.

**Start-match RES body (`sub_43D9B0`, wire builder = `inject_start_res` parity):**

| off | type | use |
|-----|------|-----|
| +2 | u16 | result (0 = success) |
| +4 | WCHAR[] | optional version/map name → `sub_65C1C0(body+4, mode@+0x26)` |
| +0x26 | u16 | game mode |
| +0x28 | u32 | → `flt_1C25E2C+0x210` |

**Create-room burst:** `build_create_room_res_burst()` → `0x3F30` + `0x3ED4` + `0x3ED8` (242 B total). Example wire prefix: `13 57 01 0b  02 30 3f  00 00 00 00 00 00 00 00  …`.

**Trigger (server cannot parse C2S RMI inside `0x25`):** after `lobby_replay.json` bursts are exhausted, reply when GAME C2S `0x25` blob length matches **`CREATE_ROOM_RES_ON_25_BODY_LEN=115`** (run 184) or session **N** (`CREATE_ROOM_RES_ON_SESSION_N`). Config in `server/.env` (`GENERATE_CREATE_ROOM_RES_ON_REQ=true`).

**Critical:** send the chain as **one TCP write** (`CREATE_ROOM_RES_AS_BURST=true`), matching `lobby_replay.json` - not three separate `sendall`s.

**Start-match trigger:** `GENERATE_START_MATCH_RES_ON_REQ=true` in `server/.env`. C2S is opaque - use `START_MATCH_RES_ON_25_BODY_LEN` (start REQ `0x3F3D` len=2) or `START_MATCH_RES_ON_READY_25_BODY_LEN` (Ready `0x3F2B` len=3) once measured on `wire_*.log`, or default `START_MATCH_RES_AFTER_CREATE_NTH_25=2` (skip room-enter `0x3ED3`, fire on first Ready after wire create-room RES). Requires `START_MATCH_REQUIRE_CREATE_ROOM_RES=true` unless disabled.

**Leave-room trigger:** `GENERATE_LEAVE_ROOM_RES_ON_REQ=true`. `LEAVE_ROOM_RES_ON_25_BODY_LEN=19` (measured run 187; **[WIP]** same as enter/ready). Alternatives: `LEAVE_ROOM_RES_ON_SESSION_N`, `LEAVE_ROOM_RES_AFTER_CREATE_NTH_25` (Nth `0x25` after wire create-room RES). Checked after create-room and start-match handlers in `game_transport.py`.

**Verify:** `cd server && uv run python -m server.test_wire_create_room` and `uv run python -m server.test_wire_start_match`; live: Create Room → Ready, check TX for `023d3f` and `game_state` → map load. Use default **debug** DLL (inject compiled out) to prove wire-only.

**2017 FA captures (`GitS_FA_Network_Captures/`):** plaintext S2C samples for `0x3F3E` (640 B), `0x3F3D`, `0x3F41`; create/list room ids were **not** on wire as bare `0x02` (likely only inside `0x25`).

**Offline emu (`lobby_replay.json` burst 1):** `0x3F3E` body is **644 B** (652 B Proud frame with 2-byte length tag). Python builder defaults to byte-exact `LOBBY_CONNECT_3F3E_BODY`; optional `name=` patches wchar at **+0x10** (`NET_USER_CONNECT_RES_NAME_OFF`). The prior **384 B** stub only covered IDA-documented fields through +0x17C.

---

## 11. In-process injection technique (our harness) **[V]**

Because C2S is opaque on the wire (§1), the server can't trigger on a click. We instead detect the REQ at the send hook and run the matching **S2C RES leaf directly**, in-process, on the receive thread. Files: [`src/RMI/Inject.cpp`](../src/RMI/Inject.cpp) (+ [`include/RMI/Inject.hpp`](../include/RMI/Inject.hpp)), wired into [`GameSendHook.cpp`](../src/RMI/GameSendHook.cpp) (latch) and [`game_state.cpp`](../src/hooks/game_state.cpp) (main-thread pump).

Recipe (direct leaf call):

```c
void *arg[8] = {0};
arg[2] = body;                  // a1+8 → flat body buffer (result u16 @ body+2 = 0)
((int(__stdcall*)(void*))game_va(leaf_rva))(arg);
```

- **Latch** on the C2S send hook (`sub_65AEA0`): when REQ id == target, set a pending flag (any thread; `InterlockedExchange`).
- **Pump** on **main thread** from `game_state` hooks (`room_list` / `room` `onPreProcess` prologues in [`game_state.cpp`](../src/hooks/game_state.cpp)) - not from `pn_drain_recv` (worker-thread pump raced the UI VM). Default **debug** build has inject off; reconfigure `-DTHEGAME_DISABLE_RMI_INJECT=OFF` to test in-process RES.

**Verified result (run 180):** create-room REQ `0x3F30` → `inject: create-room RES 0x3F30 result=0 -> GameRoom` → `game_state: room`. Client entered `CGameRoom`. **[V]**

### Threading note **[V]**

Injection ran on tid 32880 (run 180) / 13432 (run 181) - a **worker/receive thread**, not the main thread (ctor tid 28308). The leaf's `RequestState(9)` is processed; the **main thread** then runs `CGameRoom::onPreProcess` (which emits our `room` stage).

### Known crash / open issue (run 181) **[V/S]**

After the bare create injection forced scene 9, the **main thread** crashed `0xC0000005` at **`0x1307B36`** in `sub_1307B30` (`mov edi,[ecx+40h]`, `ecx` garbage) - a generic serialization helper (`sub_1307B30` → `sub_1305450`/`sub_11BC440`). Suspicion: `CGameRoom` dereferences a room/match object that a real server would have populated via room-state RESes (`0x3F81 sub_4BB450`, member list `0x3F11 sub_4BB7E0`, enter-ack `0x3ED4 sub_4BB560`) which we did **not** inject. Run 180 survived by timing luck. **Fix path:** inject the room-state RES(es) to populate the room object before/with the transition (under investigation).

---

## 12. Open questions / suspicions / corrections

- **CORRECTION:** [proudnet-rmi-server-plan.md](proudnet-rmi-server-plan.md) "Action → REQ id" table lists `sub_6653B0` as the **NetUserConnect** REQ (id `0x3F3D`). Per decompile this session, **`sub_6653B0` is the game-START sender** (proxy `0x3F3D`/16189, floor 15017), called from `ClickGameStart sub_43C0D0`. The connect REQ sender is elsewhere (connect RES is `0x3F3E sub_4BA070`; its REQ is not `sub_6653B0`). Treat the plan-doc row as stale. **[V]**
- `flt_1C25E2C` - what manager/object? Its `+0x210` receives the start RES `+0x28` dword. If null in the waiting room, the start RES won't transition. Need to find who sets `flt_1C25E2C` (many writers in `0x82xxxx–0x8Axxxx`; likely match/play-context ctor). **[S]**
- `dword_1C1A980` (map id read by CBasePlayLoading) - no direct writers found via xref; likely written through a computed pointer or set from room config. **[S]**
- Server **HostID** for injected `CReceivedMessage +0x1C`: assumed `1` (server fast-path in `sub_D00FE0`). Confirm by logging `+0x1C` on a real incoming S2C. **[S]**
- `0x3F2B` Ready RES id (`0x3F2C`?) and the `10246`-byte `0x3AC6` player block layout are uncharacterized. **[S]**
- Quick-match / Quick Dive (`0x3EE4`) RES path not yet RE'd. **[S]**
- Map-load entry is **gated on a connect to a dedicated game server** per the stage table note; reaching scene 11 may stall at the loading screen without that connection. Reaching the loading screen itself satisfies the current milestone. **[S]**

---

## RVA index

| RVA | Symbol / role |
|-----|---------------|
| `0x65AEA0` | C2S send proxy wrapper (explicit id, 16xxx) |
| `0xA0B290` | C2S floor transmit (id=`*(u16)msg`, 15xxx) |
| `0xD5C5E0` | framework `RmiSend` (heartbeat 1001/1006/1019 only) |
| `0x1C1ABA0` / `0x1C1ABB0` | proxy singletons (account-lobby-room / match-party) |
| `0xD65940` | receive drain `OnMessageReceived` (resume `0xD65947`) |
| `0xD653B0` | `ProcessMessage_ProudNetLayer` dispatch |
| `0xD59300` / `0xD58B30` / `0xD589C0` | Message_Read / CMessage::Read / restore offset |
| `0xD64F10` | RMI handler (read id, dispatch to leaf) |
| `0xD00FE0` | peer resolver (HostID→peer; `1`=server) |
| `0xD12450` / `0xD58860` / `0xD12170` / `0xD124D0` | CReceivedMessage ctor / base / get-data / dtor |
| `0x4B8630` (binder `0x556CB0`) | CAccountPacket::Register |
| `0x421070` (binder `0x554930`) | CCommonPacket::Register |
| `0x4366E0` (binder `0x555520`) | room proxy registrar (0x3F2F/30/31) |
| `0x43A680` (binder `0x5555E0`) | floor/match registrar (incl 0x3F3D start) |
| `0x4BA070` | connect RES `0x3F3E` leaf |
| `0x437160` / `0x437390` / `0x437240` | create / list / close RES leaves |
| `0x4BB560` / `0x4BB450` / `0x4BB7E0` / `0x4BB8F0` / `0x4BAEB0` | room-enter / room-state / member-list / counter / record RES leaves |
| `0x43D9B0` | game-start RES `0x3F3D` leaf |
| `0x43D020` | leave-room RES `0x3F45` leaf |
| `0x665400` | leave-room REQ sender (proxy `0x3F45`, floor `0x3AA3`, len=6) |
| `0x48A7C0` | ClickCreateRoom (REQ builder) |
| `0x6653B0` / `0x665940` / `0x43C0D0` | start sender / alt start sender / ClickGameStart UI |
| `0x41F0D0` | `CStateMachine::RequestState(scene)` |
| `0x41A920` | scene→class factory |
| `0x1C155C0` (+128/+132/+140) | CStateMachine base / active / current scene / next scene |
| `0x675160` | server-error popup |
| `0x49E530` / `0x40B340` / `0x670D20` | scene-transition fade helpers |
| `0x65C1C0` | start-RES wstring copy + mode set (`dword_1C1D960`, `word_1C1D982`) |
| `0x1C25E2C` (+0x210) | match/play context ptr (set from start RES +0x28) |
| `0x1C1A980` | map id (read by CBasePlayLoading) |
| `0x4806E0` / `0x47F610` / `0x6D9C70` | CBasePlayLoading onPreProcess / CGamePlay onPreProcess / begin-load |
| `0x1307B30` | serialization helper (run-181 crash site) |

---

*Sources: live runs 179 (`ctl/logs/runs/179_9e114d79`), 180 (`180_b5f3006c`), 181 (`181_6c4eb2e0`); IDA MCP GAME @13337 decompiles 2026-05-29.*
