# Game RMI - IDA / reimpl index

GAME.exe RVAs for the **application RMI layer**. Image base `0x400000`, IDA port **13337**. Client flow: [rmi/client.md](rmi/client.md). Server builders: [rmi/server.md](rmi/server.md).

---

## C2S send

| RVA | Symbol | Hook | Mode |
| --- | --- | --- | --- |
| `0x65AEA0` | C2S proxy wrapper (16xxx, explicit id) | `src/RMI/GameSendHook.cpp` | **trace** |
| `0xA0B290` | C2S floor transmit (15xxx, id in msg) | `src/RMI/GameSendHook.cpp` | **trace** |
| `0xD5C5E0` | Framework `RmiSend` (heartbeat only) | `src/RMI/FrameworkSendHook.cpp` | **trace** |

Globals: `0x1C1ABA0` (account/lobby proxy), `0x1C1ABB0` (match floor).

---

## S2C receive / dispatch

| RVA | Symbol | Hook | Mode |
| --- | --- | --- | --- |
| `0xD65940` | Receive drain | `pn_drain_recv.cpp` | **trace** (resume `0xD65947`) |
| `0xD653B0` | `ProcessMessage_ProudNetLayer` | `pn_process_message.cpp` | **full jmp** |
| `0xD64F10` | RMI case handler | - | tail-call GAME |
| `0xD59300` / `0xD58B30` | Message_Read / CMessage::Read | - | tail-call GAME |
| `0xD00FE0` | Peer resolver | - | - |
| `0xD12450` / `0xD124D0` | CReceivedMessage ctor/dtor | - | - |

---

## Registrars

| RVA | Registrar | Binder | Count |
| --- | --- | --- | --- |
| `0x4B8630` | `CAccountPacket::Register` | `0x556CB0` | ~55 |
| `0x421070` | `CCommonPacket::Register` | `0x554930` | ~43 |
| `0x4366E0` | Room proxy | `0x555520` | 3 |
| `0x43A680` | Floor/match | `0x5555E0` | 36 |

---

## Key RES leaves (connect / lobby / room)

| RMI id | Leaf RVA | Role | Hook / inject |
| --- | --- | --- | --- |
| `0x3F3E` | `0x4BA070` | NetUserConnectRES | - |
| `0x3E8E` | `0x4BA520` | NetConnectRES | - |
| `0x3F2F` | `0x437390` | Room-list RES | - |
| **`0x3F30`** | **`0x437160`** | **Create-room RES** | inject @ `src/RMI/Inject.cpp` |
| `0x3F31` | `0x437240` | Room close | - |
| `0x3ED4` | `0x4BB560` | Room-enter ack | inject WIP |
| `0x3ED8` | `0x4BB2E0` / `0x4BB370` | Member UI map | inject WIP |
| `0x3F81` | `0x4BB450` | Room state | - |
| `0x3F11` | `0x4BB7E0` | Member name | - |
| **`0x3F3D`** | **`0x43D9B0`** | **Start-match RES** | inject WIP |
| **`0x3F45`** | **`0x43D020`** | **Leave-room RES** | - |
| `0x675160` | - | Server-error popup | - |

---

## Key REQ senders

| RVA | Action | Proxy id | Floor id |
| --- | --- | --- | --- |
| `0x48A7C0` | ClickCreateRoom | - | (in 98 B body) |
| `0x6653B0` | Game start | `0x3F3D` | `0x3AA9` |
| `0x665400` | Leave room | `0x3F45` | `0x3AA3` |
| `0x665940` | Alt start | `0x3F3A` | `0x3A9E` |
| `0x43C0D0` | ClickGameStart UI | - | - |

---

## State machine

| RVA | Role |
| --- | --- |
| `0x41F0D0` | `CStateMachine::RequestState(scene)` |
| `0x41A920` | Scene → class factory |
| `0x1C155C0` | State manager base (+128 active, +132 scene, +140 next) |

### UI onPreProcess hooks (`game_state.cpp`)

| RVA | Stage |
| --- | --- |
| `0x42BD50` | `lobby` |
| `0x4362E0` | `room_list` |
| `0x439B00` | `room` |
| `0x4806E0` | `map_loading` |
| `0x47F610` | `in_game` |

Full table: [plans/proudnet-rmi-server-plan.md](plans/proudnet-rmi-server-plan.md).

---

## Injection harness

| File | Role |
| --- | --- |
| [`src/RMI/Inject.cpp`](../../src/RMI/Inject.cpp) | Build body + call leaf |
| [`src/hooks/game_state.cpp`](../../src/hooks/game_state.cpp) | Main-thread pump |
| [`src/RMI/GameSendHook.cpp`](../../src/RMI/GameSendHook.cpp) | C2S latch |

Env: **`THEGAME_DISABLE_RMI_INJECT=1`** for wire-only tests.

---

## `CAccountPacket::Register` - full id → leaf

| id | leaf | id | leaf | id | leaf |
| --- | --- | --- | --- | --- | --- |
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

## `CCommonPacket::Register` - full id → leaf

| id | leaf | id | leaf | id | leaf |
| --- | --- | --- | --- | --- | --- |
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

## Room proxy `sub_4366E0`

| RES id | leaf |
| --- | --- |
| `0x3F2F` | `0x437390` |
| **`0x3F30`** | **`0x437160`** |
| `0x3F31` | `0x437240` |

## Floor/match `sub_43A680` (selected)

| RES id | leaf | Notes |
| --- | --- | --- |
| `0x3F3D` | `0x43D9B0` | start match |
| `0x3F45` | `0x43D020` | leave room |
| `0x3F32` | `0x43CCC0` | |
| `0x3F46` | `0x43CE00` | |

*Last updated: 2026-05-29 - tables from IDA decomp; living notes in [plans/proudnet-game-rmi.md](plans/proudnet-game-rmi.md).*
