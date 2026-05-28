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

**GAME:** find analogous switch by xref to **`Message_Read`** / RMI dispatch (`sub_4B8630`), not by transport opcode.

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
