# ProudNet SDK ↔ GAME.exe crossmap (historical)

Maps **structures, enums, and signatures** between bundled ProudNet **v1.8** sample DLL and statically linked **~1.5** code in `GAME.exe`. For current wire specs see [proudnet/protocol.md](proudnet/protocol.md); for GAME-only RVAs see [proudnet-ida-reimpl.md](proudnet-ida-reimpl.md).

| IDA instance | Port | Binary | Role |
| --- | --- | --- | --- |
| **GAME** | 13337 | `GAME.exe` | **Target** - RVAs in GAME column |
| **PN18** | 13338 | `ProudNet/ProudNet/lib/Win32/dll/Debug/ProudNetClient.dll` | v1.8, 32-bit, **PDB** |
| **PN17** | 13339 | `MSB_Data/Plugins/ProudNetClient.dll` | v1.7, x64, stripped |

Rebase: GAME `0x400000`, PN18 `0x10000000`, PN17 `0x180000000`. Do **not** mix PN17 and PN18 in one database.

**Note:** v1.8 `MessageType` value **47** = `MessageType_Compressed` - unrelated to transport opcode **`0x2F` (47)** hello. Enum ordinals ≠ transport opcodes.

---

## TCP layer - splitters & framing

### Splitter constants (`CTcpLayer_Common`)

| Decimal | Hex | v1.8 (`Extract` @ `0x100e7710`) | GAME (`sub_D84BB0`) |
| --- | --- | --- | --- |
| 22291 | `0x5713` | Primary splitter | `LOWORD == 22291` (plain on offline path) |
| 22547 | `0x5813` | Alternate | - |
| 22292 | `0x5714` | Splitter + messageID | - |
| 2313 | `0x0909` | simplePacketMode | - |

PN18 obfuscation: `RandomToSplitter` / `SplitterToRandom` @ `0x1008a510` / `0x10091f10`. GAME offline path uses plaintext **`0x5713`**.

### Function correspondence

| Role | GAME RVA | PN18 symbol | PN18 RVA |
| --- | --- | --- | --- |
| Recv frame parser | `0xD84BB0` | `CTcpLayerMessageExtractor::Extract` | `0x100e7710` |
| Send frame builder | `0xD84970` | `AddSplitterButShareBuffer` family | `0x100d4280`+ |
| Read varint | `0xD59250` | in Extract | - |
| Write varint | `0xD59DB0` | send path | - |
| Write magic | `0xD85740` | `SplitterToRandom(0x5713)` | - |

PN17 Extract analogue: `sub_1800641C0`.

### `CTcpLayerMessageExtractor` (PN18 offsets)

| Offset | Field |
| --- | --- |
| `0x00` | `m_recvStream` |
| `0x04` | `m_recvStreamCount` |
| `0x08` | `m_extractedMessageAddTarget` |
| `0x0C` | `m_senderHostID` |
| `0x10` | `m_messageMaxLength` |
| `0x14` | `m_outLastSuccessOffset` |
| `0x18` | `m_remoteAddr_onlyUdp` |

---

## `CFastSocket` layout (PN18 → GAME)

PN18 size **264** (`0x108`). GAME object larger (~328+ B).

| PN18 offset | PN18 member | GAME offset | Evidence |
| --- | --- | --- | --- |
| **`0x6C`** | **`m_recvOverlapped`** | **`0x74`** | recv_complete |
| **`0x80`** | **`m_sendOverlapped`** | **`0xA8`** | WSASend |
| **`0x94`** | **`m_recvFlags`** | **`0xF8`** | WSARecv flags |
| **`0xB8`** | **`m_recvBuffer`** | **`0x8C`** | growable recv |
| **`0xD4`** | **`m_sendBuffer`** | **`0xC0`** | growable send |
| **`0x104`** | **`m_socket`** | **`0x12C`** | primary socket |
| - | - | **`0xE4`** | AddrPort update |

**Rule:** PN18 `m_recvOverlapped`…`m_sendBuffer` → add **+8**; `m_sendOverlapped`…`m_socket` → add **+0x28**.

| Role | GAME | PN18 symbol | PN18 RVA |
| --- | --- | --- | --- |
| Fast send | `0xD567F0` | `CFastSocket::IssueSend` | `0x101343d0` |
| Fast recv | `0xD56470` | `CFastSocket::IssueRecv` | `0x10134260` |
| Recv complete | `0xD55510` | overlap completion | - |

---

## Connection FSM

| State | GAME RVA | PN18 analogue |
| --- | --- | --- |
| 0 connect | `0xD6EAC0` | bind/connect path |
| 1 select/send | `0xD6EF90` | - |
| 2 send + recv | `0xD6C8E0` | - |
| 3 recv complete | `0xD6C9C0` | - |
| 4 finish | `0xD6DC70` | - |
| Dispatch | `0xD6F7B0` | connection driver |
| Worker | `0xD668B0` | `"PNCliWorker"` |

FSM state @ `conn+0x40` (`PNConnectionNode::kFsmState` in `pn_layout.hpp`).

---

## `CNetClient` sizes

| Type | GAME | PN18 |
| --- | --- | --- |
| `CNetClientImpl` | `new(0x17A8)` = 6056 | 5384 |
| `CNetClientManager` | `0xD0` = 208 | 368 |
| Factory / ctor | `0xD0C0A0` / `0xD0A340` | - |

---

## `Proud::MessageType` (v1.8 PDB)

From `ProcessMessage_ProudNetLayer` @ `0x1005a370`:

| Value | Enumerator |
| --- | --- |
| 1 | `MessageType_Rmi` |
| 2 | `MessageType_UserMessage` |
| 3 | `MessageType_Hla` |
| 4 | `MessageType_ConnectServerTimedout` |
| 5 | `MessageType_NotifyStartupEnvironment` |
| 11 | `MessageType_NotifyProtocolVersionMismatch` |
| 12 | `MessageType_NotifyServerDeniedConnection` |
| 13 | `MessageType_NotifyServerConnectSuccess` |
| 15–17, 19 | recovery / holepunch |
| 21, 23 | UDP matched / holepunch ack |
| 25–42 | UDP / relay / ping |
| 47 | `MessageType_Compressed` |
| 54–56 | multicast / linger |

GAME per-case RVAs: [proudnet/message-dispatch.md](proudnet/message-dispatch.md). **GAME ↔ PN18 `MessageType` send-path table:** [proudnet/message-type-crossmap.md](proudnet/message-type-crossmap.md).

### v1.8 ↔ GAME dispatch core

| v1.8 symbol | v1.8 RVA | GAME RVA |
| --- | --- | --- |
| `Message_Read(MessageType)` | `0x10086B60` | `0xD59300` |
| `CMessage::Read` | `0x1008A5E0` | `0xD58B30` |
| `CMessage::SkipRead` | `0x10051830` | `0xD589C0` |
| `ProcessMessage_ProudNetLayer` | `0x1005A370` | `0xD653B0` |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` (GAME case **39**) |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` (GAME cases **37–38**) |
| `IsFromRemoteClientPeer` | `0x1005B4B0` | `0xD5FD30` |
| `OnMessageReceived` | `0x100BB1E0` | `0xD65940` |
| Alternate batch dispatch | - | `0xD366A0` / caller `0xD37BC0` |

### Named handler crossmap (selected)

| v1.8 symbol | v1.8 RVA | GAME RVA | Case |
| --- | --- | --- | --- |
| `ProcessMessage_Rmi` | `0x1005AC20` | `0xD64F10` | 1 |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD65170` / `0xD5FFD0` | 2 / 3 |
| `ProcessMessage_NotifyServerConnectSuccess` | `0x1005DF50` | `0xD61490` | 13 |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` | 37–38 |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` | 39 |
| `CFastSocket::IssueSend` | `0x101343D0` | `0xD567F0` | - |
| `CFastSocket::IssueRecv` | `0x10134260` | `0xD56470` | - |
| `CTcpLayerMessageExtractor::Extract` | `0x100E7710` | `0xD84BB0` | - |

Full per-case GAME table: [proudnet/message-dispatch.md](proudnet/message-dispatch.md).

### Complete v1.8 PDB → GAME.exe dispatch RVAs

#### MessageType read / dispatch core

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
| --- | --- | --- | --- |
| `Proud::Message_Read` (`MessageType`) | `0x10086B60` | `0xD59300` | Bit-align + 1-byte type via `CMessage::Read` |
| `j_?Message_Read@…MessageType…` (thunk) | `0x100079B4` | `0xD59300` | Thunk target = `Message_Read` |
| `CMessage::Read` (`unsigned char&`) | `0x1008A5E0` | `0xD58B30` | Bit-offset buffer read |
| `CMessage::SkipRead` | `0x10051830` | `0xD589C0` | Restore read offset on failure |
| `CNetClientWorker::ProcessMessage_ProudNetLayer` | `0x1005A370` | `0xD653B0` | PN18 **57** cases, GAME **50** |
| `GetWorkTypeFromMessageHeader` | `0x101355A0` | `0xD8B4A0` | type **1→1, 2→2, else 3** |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` | PN18 **43–46**; GAME **39** |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` | PN18 **47**; GAME **37–38** |
| `IsFromRemoteClientPeer` | `0x1005B4B0` | `0xD5FD30` | Peer guard |
| `OnMessageReceived` | `0x100BB1E0` | `0xD65940` | Recv-list drain |
| Alternate batch dispatch | - | `0xD366A0` | caller `0xD37BC0` |
| Connect-notify `Message_Read` | - | `0xD28660` | types **12 / 16** |

#### `sub_D653B0` per-case handlers

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
| --- | --- | --- | --- |
| `ProcessMessage_Rmi` | `0x1005AC20` | `0xD64F10` | case **1** |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD65170` | case **2** |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD5FFD0` | case **3** |
| `ProcessMessage_ConnectServerTimedout` | `0x1005CE90` | `0xD62930` | case **4** |
| `ProcessMessage_NotifyStartupEnvironment` | `0x1005CF60` | - | case **5** default |
| *(no label)* | - | `0xD62FD0` | case **6** |
| *(no label)* | - | `0xD60020` | case **8** |
| *(no label)* | - | `0xD60070` | case **9** |
| *(no label)* | - | `0xD64760` | case **10** |
| `ProcessMessage_NotifyProtocolVersionMismatch` | `0x1005EBD0` | `0xD5FF60` | case **11** |
| `ProcessMessage_NotifyServerDeniedConnection` | `0x1005EC80` | - | case **12** default |
| `ProcessMessage_NotifyServerConnectSuccess` | `0x1005DF50` | `0xD61490` | case **13** |
| `ProcessMessage_NotifyAutoConnectionRecoverySuccess` | `0x10026320` | `0xD645C0` | case **15** |
| `ProcessMessage_NotifyAutoConnectionRecoveryFailed` | `0x100268D0` | - | case **16** default |
| `ProcessMessage_RequestStartServerHolepunch` | `0x1005CCE0` | `0xD608D0` | case **17** |
| `ProcessMessage_ServerHolepunchAck` | `0x1005C5A0` | `0xD63510` | case **19** |
| `ProcessMessage_NotifyClientServerUdpMatched` | `0x1005BF90` | `0xD63750` | case **24** |
| `ProcessMessage_P2PUnreliablePing` | `0x1005B590` | `0xD60A50` | case **25** |
| `ProcessMessage_P2PUnreliablePong` | `0x1005B8C0` | `0xD63A70` | case **26** |
| *(no label)* | - | `0xD64D60` | case **29** |
| *(noop)* | - | - | cases **30, 48** |
| *(no label)* | - | `0xD5FCD0` | case **31** |
| *(no label)* | - | `0xD5FD00` | case **32** |
| *(no label)* | - | `0xD61F20` | case **33** |
| *(no label)* | - | `0xD60E10` | case **34** |
| `ProcessMessage_ReliableRelay2` | `0x1005ED90` | `0xD625E0` | case **35** |
| *(no label)* | - | `0xD61760` | case **36** |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` | cases **37–38** |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` | case **39** |
| *(no label)* | - | `0xD62110` | case **40** |
| *(no label)* | - | `0xD60FB0` | case **41** |
| *(encrypted/compressed)* | - | - | cases **42–47** default |
| *(no label)* | - | `0xD62330` | case **49** |
| *(no label)* | - | `0xD61120` | case **50** |
| `ProcessMessage_PeerUdp_ServerHolepunchAck` | `0x1005CAE0` | - | §6 case **23** default |
| `ProcessMessage_ReliableUdp_Frame` | `0x1005BCC0` | - | UDP family |
| `ProcessMessage_UnreliableRelay2` | `0x1005F2A0` | - | relay family |
| `ProcessMessage_LingerDataFrame2` | `0x1005F5C0` | - | §6 **54–56** default |
| `ProcessMessage_S2CRoutedMulticast1` | `0x10059BE0` | - | default |
| `ProcessMessage_S2CRoutedMulticast2` | `0x1005A1C0` | - | default |

#### `sub_D366A0` alternate-path handlers

| v1.8 symbol | v1.8 RVA | GAME RVA | Notes |
| --- | --- | --- | --- |
| `ProcessMessage_Rmi` | `0x1005AC20` | `0xD351F0` | case **1**, `return 0` |
| `ProcessMessage_UserOrHlaMessage` | `0x1005B0F0` | `0xD353A0` | case **2**, `return 0` |
| *(batch)* | - | `0xD2B230` | case **5** |
| *(batch)* | - | `0xD33DE0` | case **7** |
| *(batch)* | - | `0xD27780` | case **12** |
| *(batch)* | - | `0xD27F10` | case **14** |
| *(batch)* | - | `0xD27A40` | case **16** |
| `ProcessMessage_Compressed` | `0x10175FE0` | `0xD5DC10` | cases **37–38** |
| `ProcessMessage_Encrypted` | `0x101758E0` | `0xD5CA30` | case **39** |
| *(batch)* | - | `0xD297C0` | case **47** (client: default) |
| *(batch 18–23, 27–28, 40–41)* | - | `0xD29BA0`…`0xD34E80` | see message-dispatch.md |

#### TCP framing & connection worker

| v1.8 symbol | v1.8 RVA | GAME RVA |
| --- | --- | --- |
| `CTcpLayerMessageExtractor::Extract` | `0x100E7710` | `0xD84BB0` |
| `CTcpLayer_Common::AddSplitterButShareBuffer` | `0x100D4280` | `0xD84970` |
| `CTcpLayer_Common::RandomToSplitter` | `0x1008A510` | - |
| `CTcpLayer_Common::SplitterToRandom` | `0x10091F10` | `0xD85740` |
| `CMessage::ReadScalar` | *(in Extract)* | `0xD59250` |
| send varint | *(send path)* | `0xD59DB0` |
| `CFastSocket::IssueSend` | `0x101343D0` | `0xD567F0` |
| `CFastSocket::IssueRecv` | `0x10134260` | `0xD56470` |
| overlapped recv complete | - | `0xD55510` |
| `CNetClientWorker` thread | *(in manager)* | `0xD668B0` |
| `CNetClientManager` ctor | - | `0xD66A30` |
| FSM driver | - | `0xD6F7B0` |

---

## Supporting types (PN18)

| Type | Size |
| --- | --- |
| `Proud::CMessage` | 48 |
| `Proud::CReceivedMessage` | 96 |
| `Proud::CSocketBuffer` | 28 |
| `Proud::CFastArray<uint8_t,…>` | 28 |
| `Proud::AddrPort` | 24 |

---

## PN17 ↔ PN18

| Item | PN17 | PN18 |
| --- | --- | --- |
| Arch | x64 | x86 |
| Symbols | Stripped | Full PDB |
| Extract | `sub_1800641C0` | `0x100e7710` |
| Magic `0x5713` | `0x18006441c` | `0x100e785b` |

Use PN17 for control-flow parity; PN18 for names and struct layouts.

---

## IDA MCP workflow

1. `list_instances` - **13337** (GAME), **13338** (PN18), **13339** (PN17).
2. PN18: `type_query`, `list_funcs *CFastSocket*`, `*CTcpLayer*`.
3. GAME: compare function size + structure offsets.
4. PN17: find immediate **22291** → Extract analogue.

*Historical reference - last crossmap pass 2026-05-28. Current specs: [Readme.md](Readme.md).*
