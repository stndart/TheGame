# `Proud::MessageType` — GAME.exe ↔ PN18 crossmap (send-path)

IDA **GAME** @ 13337 (`GAME.exe`, base `0x400000`). IDA **PN18** @ 13338 (`ProudNetClient.dll`, base `0x10000000`). Method: outbound functions that call `Message_Write` with a compile-time `push imm8` before `j_?Message_Write@...`, filtered to **send-shaped** paths (`CSendFragRefs::Add` and/or `CSmallStackAllocMessage` + `m_core->Send` pattern).

Related: [message-dispatch.md](message-dispatch.md) (recv `sub_D653B0`), [proudnet-sdk-crossmap.md](../proudnet-sdk-crossmap.md) §6, [`include/ProudNet/MessageType.hpp`](../../include/ProudNet/MessageType.hpp).

**Do not** assume PN18 ordinals on GAME wire without a row below — several families were renumbered (compress/encrypt/connect).

---

## Methodology

| Signal | Meaning |
| --- | --- |
| **rmi_like** | `CSmallStackAllocMessage` ctor → `Message_Write` → `CSendFragRefs::Add` → `SendOpt` / `m_core->Send` (same skeleton as `IRmiProxy::RmiSend`). |
| **has_add** | Uses `CSendFragRefs::Add` (broader send/reply builders). |
| **Recv dispatch** | `CNetClientWorker::ProcessMessage_ProudNetLayer` — GAME `sub_D653B0`, PN18 `0x1005A370` — case number = wire byte for handled types. |
| **Handler crossmap** | Paired handler RVAs from sdk-crossmap §6b when send function lives in the handler. |

PN18 has fewer direct `Message_Write` call sites (many sends go through `CSuperSocket::AddToSendQueue...` without a separate symbol); recv-switch names anchor ordinals where send RVA is missing.

---

## Canonical RmiSend / core send clones

| GAME RVA | GAME symbol | GAME type | PN18 RVA | PN18 symbol | PN18 type | Confidence | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0xD5C5E0` | `ProudC2S::Proxy::RmiSend` | **1** | `0x10049D90` | `IRmiProxy::RmiSend` | **1** `MessageType_Rmi` | **A** | Framework/game C2S proxy; same layout you traced in PN18. |
| `0xD5CC10` | `sub_D5CC10` | **2** | `0x101761C0` | `CNetCoreImpl::SendUserMessage` | **2** `MessageType_UserMessage` | **A** | User-message send clone (not named `RmiSend`). |
| `0xD5D450` | `sub_D5D450` | **37** or **38** | `0x10174550` | `CNetCoreImpl::Send_CompressLayer` | **47** `MessageType_Compressed` | **B** | GAME splits compress by encrypt mode (37 vs 38); PN18 always writes **47** in compress header. |
| `0xD5E0A0` | `sub_D5E0A0` | **39** | — | `Send_SecureLayer` / encrypt family | **43-46** (four variants) | **C** | GAME collapsed encrypted wire types to **39**; PN18 recv switch uses `MessageType_Encrypted_*` ×4. Wrapper writes **39** then tail-calls `sub_D5D450`. |

`0xD5A030` / `0xD5A090` (GAME types **1** / **2**) only build stack headers for other senders — not full `RmiSend` paths.

---

## Wire ordinal map (GAME byte → PN18 name)

GAME column = byte read by `Message_Read` @ `0xD59300` / case in `sub_D653B0`. PN18 names from `ProcessMessage_ProudNetLayer` @ `0x1005A370` decompile + sdk-crossmap §6.

| Dec | Hex | GAME `sub_D653B0` | PN18 enumerator (recv switch) | Confidence | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 | `01` | Rmi → `0xD64F10` | `MessageType_Rmi` | **A** | |
| 2 | `02` | UserMessage → `0xD65170` | `MessageType_UserMessage` | **A** | |
| 3 | `03` | Hla → `0xD5FFD0` | `MessageType_Hla` | **A** | |
| 4 | `04` | ConnectServerTimedout → `0xD62930` | `MessageType_ConnectServerTimedout` | **A** | |
| 5 | `05` | **default** (unhandled) | `MessageType_NotifyStartupEnvironment` | **B** | GAME **sends** type **5** from `sub_D62930` (connect path); PN18 **sends** `MessageType_RequestServerConnection` (**6**) from `NotifyStartupEnvironment` handler @ `0x1005CF60`. Recv naming ≠ send ordinal. |
| 6 | `06` | → `0xD62FD0` | *(no client recv case)* | **C** | PN18 **send** uses **6** in `NotifyStartupEnvironment` path (`RequestServerConnection`). GAME recv case **6** exists; PN18 client switch has no case **6**. |
| 7 | `07` | default | — | **D** | GAME-only gap on PN18 client dispatch. |
| 8 | `08` | → `0xD60020` | — | **C** | GAME handler only. |
| 9 | `09` | → `0xD60070` | — | **C** | |
| 10 | `0A` | → `0xD64760` | — | **C** | |
| 11 | `0B` | → `0xD5FF60` | `MessageType_NotifyProtocolVersionMismatch` | **A** | |
| 12 | `0C` | default | `MessageType_NotifyServerDeniedConnection` | **B** | PN18 has handler; GAME default. |
| 13 | `0D` | → `0xD61490` | `MessageType_NotifyServerConnectSuccess` | **A** | |
| 14 | `0E` | default | — | **C** | GAME send `sub_D61490` uses type **14**; no PN18 recv case **14** in client switch. |
| 15 | `0F` | → `0xD645C0` | `MessageType_NotifyAutoConnectionRecoverySuccess` | **A** | |
| 16 | `10` | default | `MessageType_NotifyAutoConnectionRecoveryFailed` | **B** | |
| 17 | `11` | → `0xD608D0` | `MessageType_RequestStartServerHolepunch` | **A** | |
| 18 | `12` | default | — | **C** | GAME send `sub_D7DDB0` @ **18**. |
| 19 | `13` | → `0xD63510` | `MessageType_ServerHolepunchAck` | **A** | |
| 20 | `14` | default | — | **C** | |
| 21 | `15` | default | `MessageType_NotifyClientServerUdpMatched` | **B** | GAME send `sub_D06980` also uses **21**. |
| 22 | `16` | default | — | **C** | |
| 23 | `17` | default | `MessageType_PeerUdp_ServerHolepunchAck` | **B** | GAME send `sub_D89500` @ **23**. |
| 24 | `18` | → `0xD63750` | — | **C** | PN18 case **24** = `PeerUdp_ServerHolepunchAck` in scan; GAME maps **24** to `NotifyClientServerUdpMatched` handler — **ordinal collision across builds**. Treat **24** as unreliable without packet capture. |
| 25 | `19` | → `0xD60A50` | `MessageType_P2PUnreliablePing` | **A** | |
| 26 | `1A` | → `0xD63A70` | `MessageType_P2PUnreliablePong` | **A** | |
| 27 | `1B` | default | — | **C** | PN18 **send** `Send_BroadcastLayer...` @ **27** (three writes). |
| 28 | `1C` | default | `MessageType_UnreliablePong` | **B** | |
| 29 | `1D` | → `0xD64D60` | `MessageType_ReliableUdp_Frame` / `SendOneFrame` | **B** | PN18 send `SendOneFrame` @ **29**. |
| 30 | `1E` | noop | `MessageType_ArbitraryTouch` | **B** | |
| 31 | `1F` | → `0xD5FCD0` | — | **C** | |
| 32 | `20` | → `0xD5FD00` | — | **C** | **Max compile-time immediate** in GAME send scan (user observation: `0x32`). |
| 33 | `21` | → `0xD61F20` | `RequestServerTimeOnNeed` send @ **33** | **B** | |
| 34 | `22` | → `0xD60E10` | `SpeedHackPingOnNeed` @ **34** | **B** | |
| 35 | `23` | → `0xD625E0` | `MessageType_ReliableRelay2` | **A** | |
| 36 | `24` | → `0xD61760` | `MessageType_UnreliableRelay2` | **B** | |
| 37 | `25` | Compressed → `0xD5DC10` | — | **D** | GAME compress **on wire** for client; see also **38**. |
| 38 | `26` | Compressed → `0xD5DC10` | — | **D** | `sub_D5D450` picks **37** vs **38** from `RmiContext` encrypt mode. |
| 39 | `27` | Encrypted → `0xD5CA30` | `MessageType_Encrypted_*` (**43-46**) | **B** | GAME single encrypted ordinal. |
| 40 | `28` | → `0xD62110` | `MessageType_P2PUnreliablePing` (reply send) | **C** | PN18 **send** scan @ **40** for ping path. |
| 41 | `29` | → `0xD60FB0` | — | **C** | |
| 42-46 | `2A-2E` | default | encrypted / policy family | **C** | GAME defaults; PN18 encrypted **43-46**. |
| 47 | `2F` | default | `MessageType_Compressed` | **B** | PN18 compress on send; GAME uses **37-38** instead. |
| 48 | `30` | noop | — | **A** | |
| 49 | `31` | → `0xD62330` | — | **C** | |
| 50 | `32` | → `0xD61120` | `SendDummyOnTcp...` @ **52** | **C** | GAME highest send immediate; PN18 dummy send uses **52** (another renumber). |

---

## Per-function send table (GAME → PN18)

All rows: GAME functions with `Message_Write(imm8)` + `CSendFragRefs::Add`. Sorted by GAME type.

| GAME type | GAME send RVA | GAME function | rmi_like | PN18 type | PN18 name | PN18 send RVA | Conf | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `0xD5C5E0` | `ProudC2S::Proxy::RmiSend` | yes | 1 | `MessageType_Rmi` | `0x10049D90` | A | |
| 2 | `0xD5CC10` | `sub_D5CC10` | yes | 2 | `MessageType_UserMessage` | `0x101761C0` | A | |
| 3 | `0xD25AA0` | `sub_D25AA0` | | 3 | `MessageType_Hla` | — | C | Alternate batch path; no PN18 `Message_Write` twin found. |
| 4 | `0xD297C0` | `sub_D297C0` | | 4 | `MessageType_ConnectServerTimedout` | — | C | |
| 5 | `0xD62930` | `sub_D62930` | | 6 | `MessageType_RequestServerConnection` | `0x1005CF60` | B | GAME writes **5**; PN18 connect packet writes **6** in same handler class. |
| 6 | `0xD2B230` | `sub_D2B230` | | 6 | *(see above)* | — | C | |
| 7 | `0xD62FD0` | `sub_D62FD0` | | — | — | — | C | |
| 8 | `0xD284F0` | `sub_D284F0` | | — | — | — | C | |
| 9 | `0xD29430` | `sub_D29430` | | — | — | — | C | |
| 9 | `0xD33DE0` | `sub_D33DE0` | | — | — | — | C | Second site @ same type. |
| 10 | `0xD2F950` | `sub_D2F950` | | — | — | — | C | |
| 11 | `0xD26EC0` | `sub_D26EC0` | | 11 | `MessageType_NotifyProtocolVersionMismatch` | — | B | |
| 13 | `0xD27780` | `sub_D27780` | | 13 | `MessageType_NotifyServerConnectSuccess` | — | B | |
| 14 | `0xD61490` | `sub_D61490` | | — | — | — | C | |
| 15 | `0xD27F10` | `sub_D27F10` | | 15 | `MessageType_NotifyAutoConnectionRecoverySuccess` | `0x10024CA0` | B | PN18 **14** @ recovery TCP helper (not same type). |
| 17 | `0xD27A40` | `sub_D27A40` | | 17 | `MessageType_RequestStartServerHolepunch` | — | B | |
| 18 | `0xD7DDB0` | `sub_D7DDB0` | | — | — | — | C | |
| 19 | `0xD9D1B0` | `sub_D9D1B0` | | 19 | `MessageType_ServerHolepunchAck` | `0x1005C5A0` | B | PN18 handler; send may differ. |
| 21 | `0xD06980` | `sub_D06980` | yes | 21 | `MessageType_NotifyClientServerUdpMatched` | — | B | |
| 23 | `0xD89500` | `sub_D89500` | yes | 23 | `MessageType_PeerUdp_ServerHolepunchAck` | `0x101418F0` | B | |
| 24 | `0xD34910` | `sub_D34910` | yes | 24 | `PeerUdp_ServerHolepunchAck` (PN18) | `0x101418F0` | C | Ordinal **24** semantics differ — verify on wire. |
| 25 | `0xD2D450` | `sub_D2D450` | | 25 | `MessageType_P2PUnreliablePing` | `0x100E55E0` | B | PN18 **25** = `BuildSendDataFromFrame`. |
| 26 | `0xD34E80` | `sub_D34E80` | yes | 26 | `MessageType_P2PUnreliablePong` | — | B | |
| 27 | `0xCFEA80` | `sub_CFEA80` | | 27 | broadcast relay | `0x100B3ED0` | B | |
| 28 | `0xCFEC40` | `sub_CFEC40` | | 28 | `MessageType_UnreliablePong` | — | B | |
| 29 | `0xD27D20` | `sub_D27D20` | | 29 | Reliable UDP frame | `0x100E5060` | B | |
| 30 | `0xD261A0` | `sub_D261A0` | | 30 | `MessageType_ArbitraryTouch` | — | B | |
| 34 | `0xD61F20` | `sub_D61F20` | | 33 | server time request | `0x100B69C0` | C | Ordinal shift **34↔33**. |
| 35 | `0xD30F40` | `sub_D30F40` | | 35 | `MessageType_ReliableRelay2` | — | B | |
| 38 | `0xD5D450` | `sub_D5D450` | yes | 47 | `MessageType_Compressed` | `0x10174550` | B | Also **37** on GAME when encrypt mode = fast. |
| 39 | `0xD5E0A0` | `sub_D5E0A0` | yes | 43-46 | `MessageType_Encrypted_*` | — | C | GAME **39** only. |
| 40 | `0xD956C0` | `sub_D956C0` | | 40 | P2P ping send | `0x1005B590` | C | |
| 41 | `0xD282E0` | `sub_D282E0` | | 41 | — | — | C | |
| 46 | `0xD2CA20` | `sub_D2CA20` | | 46 | — | — | C | |
| 47 | `0xD619C0` | `sub_D619C0` | | 47 | `MessageType_Compressed` | — | B | GAME still has **47** on some paths; client recv uses **37-38**. |
| 49 | `0xCFEDC0` | `sub_CFEDC0` | | 49 | — | — | C | |
| 50 | `0xD62330` | `sub_D62330` | | 52 | TCP dummy | `0x100F3A40` | C | **50↔52** renumber. |

**Not listed:** `sub_D5A030` / `sub_D5A090` (header-only), `sub_D03970` / `sub_D7E030` / `sub_D7E1E0` / `sub_D7E460` (stack builders), runtime-type wrappers (`sub_D880F0`, `sub_CFD140`, ...).

---

## PN18 send sites without a GAME twin (scan)

| PN18 type | PN18 RVA | PN18 symbol |
| --- | --- | --- |
| 6 | `0x1005CF60` | `ProcessMessage_NotifyStartupEnvironment` (contains `RequestServerConnection` write) |
| 14 | `0x10024CA0` | `AutoConnectionRecovery_OnTcpConnection` |
| 18 | `0x100B57E0` | `SendServerHolepunch` |
| 20 | `0x1005C5A0` | `ProcessMessage_ServerHolepunchAck` (reply) |
| 22 | `0x1013FCC0` | `Heartbeat` |
| 37-38 | `0x1013F700` / `0x1013F930` | holepunch send/ack |
| 39 | `0x10063D70` | `P2PPingOnNeed` |
| 42 | `0x10059BE0` | `ProcessMessage_S2CRoutedMulticast1` |
| 47 | `0x10174550` | `Send_CompressLayer` |
| 52 | `0x100F3A40` | `SendDummyOnTcpOnTooLongUnsending` |
| 55-56 | `0x10063D70` / `0x1005FB00` | P2P reliable ping/pong |

---

## Confidence legend

| Code | Meaning |
| --- | --- |
| **A** | Same type integer + structural match (RmiSend clone) or both dispatch switches agree. |
| **B** | Same type integer + semantic/name agreement; send RVAs may differ or PN18 uses socket queue only. |
| **C** | Semantic guess from handler crossmap or one-sided send site; ordinals may disagree. |
| **D** | Known renumber (compress/encrypt/connect) or GAME/PN18 default mismatch. |

---

## Takeaways for RE

1. **`0xD5C5E0` ↔ `0x10049D90`** @ type **1** is the correct mental model for framework `RmiSend` (your 90% case).
2. **Game PIDL traffic** does not use that RVA — it uses `sub_65AEA0` / `sub_A0B290` without going through `Message_Write` in the hot path for many proxies.
3. **Do not copy PN18 enum integers wholesale** into GAME server frames: remap **37-38↔47** and **39↔43-46**, and validate **5/6** connect and **24** holepunch families on captured bytes.
4. Update [`include/ProudNet/MessageType.hpp`](../../include/ProudNet/MessageType.hpp) when a row promotes from **C→A** via wire capture.

*Last updated: 2026-05-30 (IDA MCP scan GAME + PN18).*
