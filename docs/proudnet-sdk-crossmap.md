# ProudNet SDK ↔ GAME.exe cross-map

Maps **structures, enums, and signatures** between:

| IDA instance | Port | Binary | Role |
|--------------|------|--------|------|
| **GAME** | 13337 | `GAME.exe` (static ~1.7) | **Target** — RVAs in this doc’s GAME column |
| **PN18** | 13338 | `ProudNet/ProudNet/lib/Win32/dll/Debug/ProudNetClient.dll` | **v1.8**, 32-bit, **PDB / full symbols** (`Release-1.8.00001-master` paths in asserts) |
| **PN17** | 13339 | `MSB_Data/Plugins/ProudNetClient.dll` | **v1.7**, **x64**, stripped (same major; logic reference only) |

Do **not** mix PN17 and PN18 in one IDA database. Rebase: GAME `0x400000`, PN18 `0x10000000`, PN17 `0x180000000`.

**Companion docs:** [proudnet-ida-symbol-map.md](proudnet-ida-symbol-map.md) (GAME hooks/RVAs), [proudnet-offline-protocol.md](proudnet-offline-protocol.md) (wire bytes).

---

## 1. Two different “message types” (why v1.8 `MessageType` ≠ wire `0x2F`)

| Layer | Name in v1.8 PDB | On wire / in GAME | Example |
|-------|------------------|-------------------|---------|
| **TCP framing** | `CTcpLayer_Common` splitters after optional obfuscation | Magic `13 57` → `0x5713`, varint length | [protocol §3](proudnet-offline-protocol.md) |
| **ProudNet envelope** | `Proud::MessageType` (read via `Message_Read`) | **Not** the first byte of offline hello | `MessageType_Rmi` = **1**, not `0x2F` |
| **Game RMI** | (game code) | Inner payload when transport opcode `0x02` | `rmi_id` `0x3F3E` |

Offline hello uses transport opcode **`0x2F` (47)**. In v1.8, **`MessageType` value 47** is **`MessageType_Compressed`** in `CNetClientWorker::ProcessMessage_ProudNetLayer` — unrelated to hello. **Do not map `CMessageType` / `MessageType` enum ordinals to transport opcodes.**

Transport opcodes for offline path: [proudnet-offline-protocol.md §4.1](proudnet-offline-protocol.md).

---

## 2. TCP layer: splitters, framing, obfuscation

### 2.1 Splitter constants (`CTcpLayer_Common`)

| Decimal | Hex | v1.8 (`Extract` @ `0x100e7710`) | GAME (`sub_D84BB0` @ `0xD84BB0`) |
|---------|-----|----------------------------------|-------------------------------------|
| 22291 | `0x5713` | Primary splitter; `ReadScalar` length | Compared **directly** as `LOWORD == 22291` (no `RandomToSplitter`) |
| 22547 | `0x5813` | Alternate splitter | (not seen in GAME parser snippet) |
| 22292 | `0x5714` | Splitter + following `messageID` | — |
| 2313 | `0x0909` | `simplePacketMode` only | — |

**PN17** (`sub_1800641C0`, ~2331 B): same immediate cluster at `0x18006441c` as PN18 — same splitter set, x64.

**Obfuscation (PN18 only on extract path):** wire may carry **randomized 16-bit** value; `RandomToSplitter` / `SplitterToRandom` @ `0x1008a510` / `0x10091f10`. **GAME offline path uses plaintext magic `0x5713`** on the socket (matches dummy server / hooks).

### 2.2 Function correspondence (framing)

| Role | GAME RVA | Size | PN18 symbol | PN18 RVA | Size | PN17 (inferred) |
|------|----------|------|-------------|----------|------|-----------------|
| Recv frame parser | `0xD84BB0` | `0x30c` | `CTcpLayerMessageExtractor::Extract` | `0x100e7710` | `0x707` | `sub_1800641C0` (~2331 B) |
| Send frame builder | `0xD84970` | `0xb2` | (inline / `AddSplitterButShareBuffer` family) | `0x100d4280`+ | — | — |
| Read varint length | `0xD59250` | — | `CMessage::ReadScalar` in extract | (in Extract) | — | — |
| Write varint length | `0xD59DB0` | — | used from send path | — | — | — |
| Write magic | `0xD85740` | — | `SplitterToRandom(0x5713)` pushed in send | — | — | — |

GAME parser is a **slim stream consumer** (plain magic + varint); PN18 **Extract** is full **message extractor** (splitters + `CReceivedMessageList`).

### 2.3 `CTcpLayerMessageExtractor` layout (PN18, apply +0 when porting to GAME stream objects)

| Offset | Field | Type |
|--------|-------|------|
| `0x00` | `m_recvStream` | `const uint8_t*` |
| `0x04` | `m_recvStreamCount` | `int` |
| `0x08` | `m_extractedMessageAddTarget` | `CReceivedMessageList*` |
| `0x0C` | `m_senderHostID` | `HostID` |
| `0x10` | `m_messageMaxLength` | `int` |
| `0x14` | `m_outLastSuccessOffset` | `int` |
| `0x18` | `m_remoteAddr_onlyUdp` | `AddrPort` (24 B) |

---

## 3. `CFastSocket` layout (PN18 → GAME)

PN18 `Proud::CFastSocket` size **264** (`0x108`). GAME fast-socket object is **larger** (~328+ B); offsets below are from **hooked GAME code** (`w_wsasend_1` / `w_wsarecv_1` / `sub_D55510`) verified May 2026.

| PN18 offset | PN18 member | GAME offset | GAME evidence |
|-------------|-------------|-------------|---------------|
| `0x0C` | `m_enableBroadcastOption` | — | — |
| `0x10` | `m_socketClosed_CS` | — | — |
| `0x54` | `m_bindAddress` (`AddrPort`) | — | — |
| **`0x6C`** | **`m_recvOverlapped`** | **`0x74` (116)** | `WSARecv` overlapped; `recv_complete` checks `this+0x74 == 259` |
| **`0x80`** | **`m_sendOverlapped`** | **`0xA8` (168)** | `WSASend` overlapped |
| **`0x94`** | **`m_recvFlags`** | **`0xF8` (248)** | `WSARecv` flags ptr |
| **`0xB8`** | **`m_recvBuffer`** (`CSocketBuffer` / `CFastArray`) | **`0x8C` (140)** | `sub_9FC720(this+0x8C)` in recv |
| **`0xD4`** | **`m_sendBuffer`** | **`0xC0` (192)** | `sub_9FC720(this+0xC0)` in send |
| `0xF4` | `m_ttlMustBeRestoreOnSendCompletion` | — | — |
| **`0xF8`** | **`m_ioPendingCount`** | **`0x104` (260)?** | send path `++*(this+260)` |
| **`0x104`** | **`m_socket`** | **`0x12C` (300)** | primary `WSASend`/`WSARecv` socket |
| — | — | **`0xBC` (188)** | send-warning flag (`w_wsasend_1`) |
| — | — | **`0x88` (136)** | recv-armed flag (`w_wsarecv_1`) |
| — | — | **`0x10` (16)** | delegate/callback ptr |
| — | — | **`0xE4` (228)** | `AddrPort`; `sub_CF0460` in `recv_complete` |

**Rule of thumb:** from PN18 `m_recvOverlapped` through `m_sendBuffer`, add **+8**; from `m_sendOverlapped` through `m_socket`, add **+0x28** to get GAME offsets.

**Known GAME bugs (keep in reimpl):** retry paths use **`this+300`** (`0x12C`) for socket in some branches while primary path uses **`this+0x12C`** — same class of bug as `pn_select` @ `+300` ([symbol map §3](proudnet-ida-symbol-map.md)).

### 3.1 Socket I/O signatures

| Role | GAME | Prototype (effective) | PN18 symbol | PN18 RVA | Size |
|------|------|----------------------|-------------|----------|------|
| Fast send | `0xD567F0` `w_wsasend_1` | `int __thiscall(void* this, void* data, int len)` | `CFastSocket::IssueSend` | `0x101343d0` | `0x1c0` |
| Fast recv | `0xD56470` `w_wsarecv_1` | `int __thiscall(void* this, int len)` | `CFastSocket::IssueRecv` | `0x10134260` | `0x170` |
| Recv complete | `0xD55510` | `char __thiscall(void* this, uint8_t wait, DWORD* out)` | (overlap completion helper) | — | `0xa6` |

GAME send **`0x1ce`** vs PN18 **`0x1c0`** — same structure, minor codegen delta.

---

## 4. Connection FSM (per-connection worker)

| State | GAME RVA | PN18 analogue |
|-------|----------|---------------|
| 0 connect | `0xD6EAC0` | `CFastSocket` connect/bind path in client worker |
| 1 select/send | `0xD6EF90` | — |
| 2 send complete + recv | `0xD6C8E0` | — |
| 3 recv complete | `0xD6C9C0` | — |
| 4 finish | `0xD6DC70` | — |
| 5 teardown | (state 5 → 6) | — |
| Dispatch | `0xD6F7B0` | connection list driver |
| Worker thread | `0xD668B0` (`"PNCliWorker"`) | `CNetClientWorker` thread proc |

FSM state index: GAME `sub_D6F7B0` uses **`conn+0x40`** as state dword (see decompilation via `v6[16]` — treat as **`PNConnectionNode::kFsmState` @ 64** in [`pn_layout.hpp`](../include/game/net/pn_layout.hpp)).

---

## 5. `CNetClient` / manager (sizes)

| Type | GAME | PN18 (`type_query`) | Notes |
|------|------|---------------------|-------|
| `CNetClientImpl` | `operator new(0x17A8)` = **6056** | **5384** | GAME build is larger (+672) |
| `CNetClient` iface | returned @ `factory+44` | `CNetClient` **88** | vtable `CNetClient_vtbl` **300** B |
| `CNetClientManager` | `w_heap_alloc(0xD0)` = **208** | **368** | Different manager layout |
| Factory | `0xD0C0A0` | — | |
| Ctor | `0xD0A340` | — | Sets `Proud::CNetClientImpl` vtables (confirmed in decomp) |
| Manager ctor | `0xD66A30` | `CNetClientManager` | `CreateThread` → `0xD668B0` |

---

## 6. `Proud::MessageType` (v1.8 PDB) — internal dispatch only

From `CNetClientWorker::ProcessMessage_ProudNetLayer` @ **`0x1005a370`** (57-case switch, **`Message_Read`**):

| Value | v1.8 enumerator |
|-------|-----------------|
| 1 | `MessageType_Rmi` |
| 2 | `MessageType_UserMessage` |
| 3 | `MessageType_Hla` |
| 4 | `MessageType_ConnectServerTimedout` |
| 5 | `MessageType_NotifyStartupEnvironment` |
| 11 | `MessageType_NotifyProtocolVersionMismatch` |
| 12 | `MessageType_NotifyServerDeniedConnection` |
| 13 | `MessageType_NotifyServerConnectSuccess` |
| 15 | `MessageType_NotifyAutoConnectionRecoverySuccess` |
| 16 | `MessageType_NotifyAutoConnectionRecoveryFailed` |
| 17 | `MessageType_RequestStartServerHolepunch` |
| 19 | `MessageType_ServerHolepunchAck` |
| 21 | `MessageType_NotifyClientServerUdpMatched` |
| 23 | `MessageType_PeerUdp_ServerHolepunchAck` |
| 25–42 | Reliable/unreliable UDP, relay, ping/pong, … |
| 47 | `MessageType_Compressed` → `ProcessMessage_Compressed` |
| 54–56 | S2C multicast / linger |

Encrypted variants: `MessageType_Encrypted_Reliable_EM_Secure`, `MessageType_Encrypted_UnReliable_EM_Secure` in `ProcessMessage_Encrypted` @ `0x101758e0`.

**GAME dispatch (internal envelope):** [proudnet-message-dispatch-map.md](proudnet-message-dispatch-map.md) · [symbol map — Message dispatch](proudnet-ida-symbol-map.md).

| Role | GAME RVA | PN18 analogue |
|------|----------|---------------|
| `ProcessMessage_ProudNetLayer` | `0xD653B0` | `CNetClientWorker::ProcessMessage_ProudNetLayer` @ `0x1005A370` |
| Receive-queue drain | `0xD65940` | — |
| `Message_Read(MessageType)` | `0xD59300` | (in layer) |
| `CMessage::Read` | `0xD58B30` | `CMessage::Read` |
| Compressed unwrap | `0xD5DC10` | `ProcessMessage_Compressed` (PDB case **47**; GAME client **37–38**) |
| Encrypted unwrap | `0xD5CA30` | `ProcessMessage_Encrypted` (PDB **43–46**; GAME client **39**) |
| Alternate dispatch (server/RMI batch) | `0xD366A0` | — (caller `0xD37BC0`; not client worker layer) |

Named-case handler RVAs (`MessageType_Rmi` → `0xD64F10`, etc.): symbol map §2b and dispatch map — not by transport opcode.

### 6b. GAME.exe dispatch RVAs (complete)

Full **v1.8 PDB → GAME.exe** table for message dispatch, TCP framing, and `CFastSocket` I/O. Per-case rows match [proudnet-message-dispatch-map.md](proudnet-message-dispatch-map.md). PN18 rebased @ `0x10000000`; GAME @ image base `0x400000`.

#### MessageType read / dispatch core

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
|-------------|----------|----------|-------|
| `Proud::Message_Read` (`MessageType`) | `0x10086B60` | `0xD59300` | Bit-align + 1-byte type via `CMessage::Read` |
| `j_?Message_Read@…MessageType…` (thunk) | `0x100079B4` | `0xD59300` | Thunk target = `Message_Read` |
| `CMessage::Read` (`unsigned char&`) | `0x1008A5E0` | `0xD58B30` | Bit-offset buffer read; all `Message_Read` helpers |
| `CMessage::SkipRead` | `0x10051830` | `0xD589C0` | GAME **restore read offset** on dispatch failure (same role) |
| `CNetClientWorker::ProcessMessage_ProudNetLayer` | `0x1005A370` | `0xD653B0` | **`Message_Read` → switch** — PN18 **57** cases, GAME **50** |
| `GetWorkTypeFromMessageHeader` | `0x101355A0` | `0xD8B4A0` | After `Message_Read`: type **1→1, 2→2, else 3** |
| `ProcessMessage_Encrypted` (`CNetCoreImpl`) | `0x101758E0` | `0xD5CA30` | PN18 cases **43–46**; GAME client **case 39** only → recurse layer |
| `ProcessMessage_Compressed` (`CNetCoreImpl`) | `0x10175FE0` | `0xD5DC10` | PN18 **case 47**; GAME client **cases 37–38** → recurse layer |
| `CNetClientWorker::IsFromRemoteClientPeer` | `0x1005B4B0` | `0xD5FD30` | Peer guard on server-originated notifies (+ peer guard rows below) |
| `CNetClientImpl::OnMessageReceived` | `0x100BB1E0` | `0xD65940` | Recv-list → per-message dispatch; PN17 loop `sub_180052F80` same role |
| `CNetClientImpl::ProcessMessageOrMoveToFinalRecvQueue` | `0x100BB510` | `0xD65940` | Also calls `ProcessMessage_ProudNetLayer`; GAME drain is `sub_D65940` |
| *(alternate batch dispatch)* | — | `0xD366A0` | **`int* this`**, 48-case switch; **not** client worker layer |
| *(batch caller — RMI unsafe)* | — | `0xD37BC0` | Iterates messages → `sub_D366A0`; case **47** handled here only |
| *(connect-notify `Message_Read`)* | — | `0xD28660` | Types **12 / 16** only; xref from server connect path |

#### `sub_D653B0` per-case handlers

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
|-------------|----------|----------|-------|
| `ProcessMessage_Rmi` | `0x1005AC20` | `0xD64F10` | **case 1** — `MessageType_Rmi` |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD65170` | **case 2** — `MessageType_UserMessage` |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD5FFD0` | **case 3** — `MessageType_Hla` (short notify stub on GAME) |
| `ProcessMessage_ConnectServerTimedout` | `0x1005CE90` | `0xD62930` | **case 4** — `OutputDebugStringW` + peer guard |
| `ProcessMessage_NotifyStartupEnvironment` | `0x1005CF60` | — | **case 5** — **default** on GAME client (unhandled) |
| *(no §6 label)* | — | `0xD62FD0` | **case 6** — startup-env payload write + peer guard |
| *(no §6 label)* | — | `0xD60020` | **case 8** — `sub_D0AF20(8,…)` + peer guard |
| *(no §6 label)* | — | `0xD60070` | **case 9** — `sub_D0AF20(9,…)` + peer guard |
| *(no §6 label)* | — | `0xD64760` | **case 10** — assert / debug path + peer guard |
| `ProcessMessage_NotifyProtocolVersionMismatch` | `0x1005EBD0` | `0xD5FF60` | **case 11** + peer guard |
| `ProcessMessage_NotifyServerDeniedConnection` | `0x1005EC80` | — | **case 12** — **default** on GAME |
| `ProcessMessage_NotifyServerConnectSuccess` | `0x1005DF50` | `0xD61490` | **case 13** + peer guard |
| `ProcessMessage_NotifyAutoConnectionRecoverySuccess` | `0x10026320` | `0xD645C0` | **case 15** — handler on `CNetClientImpl` in PN18 + peer guard |
| `ProcessMessage_NotifyAutoConnectionRecoveryFailed` | `0x100268D0` | — | **case 16** — **default** on GAME |
| `ProcessMessage_RequestStartServerHolepunch` | `0x1005CCE0` | `0xD608D0` | **case 17** + peer guard |
| `ProcessMessage_ServerHolepunchAck` | `0x1005C5A0` | `0xD63510` | **case 19** + peer guard; also relay xrefs |
| `ProcessMessage_NotifyClientServerUdpMatched` | `0x1005BF90` | `0xD63750` | **case 24** + peer guard |
| `ProcessMessage_P2PUnreliablePing` | `0x1005B590` | `0xD60A50` | **case 25** + peer guard |
| `ProcessMessage_P2PUnreliablePong` | `0x1005B8C0` | `0xD63A70` | **case 26** + peer guard |
| *(no §6 label)* | — | `0xD64D60` | **case 29** + peer guard |
| *(noop)* | — | — | **cases 30, 48** — `messageProcessed=1`, no handler |
| *(no §6 label)* | — | `0xD5FCD0` | **case 31** |
| *(no §6 label)* | — | `0xD5FD00` | **case 32** |
| *(no §6 label)* | — | `0xD61F20` | **case 33** |
| *(no §6 label)* | — | `0xD60E10` | **case 34** |
| `ProcessMessage_ReliableRelay2` | `0x1005ED90` | `0xD625E0` | **case 35** |
| *(no §6 label)* | — | `0xD61760` | **case 36** |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` | **cases 37–38** — unwrap + **recurse** `sub_D653B0` |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` | **case 39** — unwrap + **recurse** `sub_D653B0` |
| *(no §6 label)* | — | `0xD62110` | **case 40** |
| *(no §6 label)* | — | `0xD60FB0` | **case 41** |
| *(encrypted/compressed)* | — | — | **cases 42–47** — **default** on client (`sub_D653B0`) |
| *(no §6 label)* | — | `0xD62330` | **case 49** |
| *(no §6 label)* | — | `0xD61120` | **case 50** |
| `ProcessMessage_PeerUdp_ServerHolepunchAck` | `0x1005CAE0` | — | §6 **case 23** — GAME **default** (block 20–23) |
| `ProcessMessage_ReliableUdp_Frame` | `0x1005BCC0` | — | §6 UDP frame family — partial ordinal overlap |
| `ProcessMessage_UnreliableRelay2` | `0x1005F2A0` | — | §6 relay family |
| `ProcessMessage_LingerDataFrame2` | `0x1005F5C0` | — | §6 **54–56** multicast/linger — **default** on GAME |
| `ProcessMessage_S2CRoutedMulticast1` | `0x10059BE0` | — | §6 S2C multicast — **default** on GAME client |
| `ProcessMessage_S2CRoutedMulticast2` | `0x1005A1C0` | — | §6 S2C multicast — **default** on GAME client |

#### `sub_D366A0` alternate-path handlers (non–ProudNet-layer)

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
|-------------|----------|----------|-------|
| `ProcessMessage_Rmi` | `0x1005AC20` | `0xD351F0` | **case 1** — **`return 0`** after handler |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD353A0` | **case 2** — **`return 0`** after handler |
| *(batch path)* | — | `0xD2B230` | **case 5** |
| *(batch path)* | — | `0xD33DE0` | **case 7** |
| *(batch path)* | — | `0xD27780` | **case 12** (assert alt `0xD32A80`) |
| *(batch path)* | — | `0xD27F10` | **case 14** |
| *(batch path)* | — | `0xD27A40` | **case 16** |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` | **cases 37–38** — shared compress helper |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` | **case 39** — shared encrypt helper |
| *(batch path)* | — | `0xD297C0` | **case 47** — **handled here only** (client: default) |
| *(batch cases 18–23, 27–28, 40–41)* | — | `0xD29BA0`…`0xD34E80` | See [dispatch map § `sub_D366A0`](proudnet-message-dispatch-map.md) |

#### TCP framing (`CTcpLayer`)

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
|-------------|----------|----------|-------|
| `CTcpLayerMessageExtractor::Extract` | `0x100E7710` | `0xD84BB0` | Recv parser — plain **`0x5713`** magic on GAME offline path |
| `CTcpLayer_Common::AddSplitterButShareBuffer` | `0x100D4280` | `0xD84970` | Send framer |
| `CTcpLayer_Common::RandomToSplitter` | `0x1008A510` | — | PN18 wire obfuscation; not used on GAME offline hello path |
| `CTcpLayer_Common::SplitterToRandom` | `0x10091F10` | `0xD85740` | GAME send pushes plain **`0x5713`** (`sub_D85740`) |
| `CMessage::ReadScalar` (in `Extract`) | *(in Extract)* | `0xD59250` | Varint length read |
| *(send varint)* | *(send path)* | `0xD59DB0` | Varint length write |

#### `CFastSocket` I/O

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
|-------------|----------|----------|-------|
| `CFastSocket::IssueSend` | `0x101343D0` | `0xD567F0` | `w_wsasend_1`; GAME size **`0x1CE`** vs PN18 **`0x1C0`** |
| `CFastSocket::IssueRecv` | `0x10134260` | `0xD56470` | `w_wsarecv_1` |
| *(overlapped recv complete)* | — | `0xD55510` | `WSAGetOverlappedResult` path; no exported `OnRecvCompleted` in PN18 listing |

#### Connection worker (context)

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
|-------------|----------|----------|-------|
| `CNetClientWorker` thread proc | *(in manager ctor)* | `0xD668B0` | `"PNCliWorker"` @ GAME; PN18 `CreateThread` in manager |
| `CNetClientManager` ctor | *(PN18 `CNetClientManager`)* | `0xD66A30` | Spawns worker thread |
| Per-connection FSM driver | — | `0xD6F7B0` | States 0–5 @ `conn+0x40` |

*Sources: IDA MCP PN18 @ 13338 + GAME @ 13337, agent transcript 4f08effa, [proudnet-message-dispatch-map.md](proudnet-message-dispatch-map.md). Last updated: 2026-05-28.*

---

## 7. Supporting types (PN18)

| Type | Size | Notes |
|------|------|-------|
| `Proud::CMessage` | 48 | Bit-stream message; `Read`/`Write`/`ReadScalar` |
| `Proud::CReceivedMessage` | 96 | `m_unsafeMessage`, `m_remoteHostID` @ `0x30`, `m_hasMessageID` @ `0x54`, `m_messageID` @ `0x50` |
| `Proud::CSocketBuffer` | 28 | Embeds `CFastArray<uint8_t,…>` |
| `Proud::CFastArray<uint8_t,0,1,int>` | 28 | Same as `PNGrowableBuffer` in GAME hooks |
| `Proud::AddrPort` | 24 | GAME `sub_CF0460` @ fast socket `+0xE4` |

---

## 8. PN17 ↔ PN18 (same major, x64 vs x86)

| Item | PN17 | PN18 |
|------|------|------|
| Arch | x64 | x86 |
| Symbols | Mostly stripped | Full MSVC mangled names |
| `Extract` analogue | `sub_1800641C0` | `0x100e7710` |
| Magic `0x5713` xrefs | `0x18006441c`, … | `0x100e785b`, … |
| `CMessage` fields (decomp) | `m_msgBuffer`, `m_isSimplePacketMode`, … | same family |

Use PN17 for **control-flow parity**, PN18 for **names, structs, and enum labels**.

---

## 9. Quick xref workflow (IDA MCP)

1. `list_instances` — pick **13337** (GAME), **13338** (PN18), or **13339** (PN17).
2. `select_instance` → `port`.
3. PN18: `type_query` / `search_structs` for layouts; `list_funcs` with `*CFastSocket*`, `*CTcpLayer*`.
4. GAME: compare **function size** + **structure offsets** in `analyze_function` / `export_funcs`.
5. PN17: `find` `immediate` → `22291` to land in Extract analogue.

---

*Last updated: 2026-05-28 — IDA instances PN17/Pn18/GAME; aligns with `pn_layout.hpp` and offline protocol doc.*
