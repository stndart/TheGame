# ProudNet - implementation map

What is **reimplemented** in this repo for the ProudNet transport path and where the code lives. For RVAs and hook modes see [../proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md). For the **current** install matrix see [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md).

---

## TheGame.dll - reimplemented C++ API

| RVA | C++ method | File | Hook mode |
| --- | --- | --- | --- |
| `0x9FC720` | `pn::growable::ensure_size` | `src/game/net/pn_growable.cpp` | in-hook |
| `0x9FCB70` | `pn::growable::calc_capacity` | `src/game/net/pn_growable.cpp` | in-hook |
| `0xD71610` | `PNRecvBuffer::ensure_capacity` | `src/game/net/pn_recv_append.cpp` | in-hook @ append |
| `0xD71CE0` | `PNRecvBuffer::append` | `src/game/net/pn_recv_append.cpp` | **full jmp** |
| `0xD55510` | `PNFastSocket::recv_complete` | `src/game/net/pn_fast_socket.cpp` | **full jmp** |
| `0xD55660` | `PNFastSocket::send_complete` | `src/game/net/pn_fast_socket.cpp` | called from reimpl |
| `0xD56470` | `PNFastSocket::recv` | `src/game/net/pn_fast_socket.cpp` | **full jmp** |
| `0xD567F0` | `PNFastSocket::send` | `src/game/net/pn_fast_send.cpp` | **full jmp** |
| `0xD55300` | `PNSelectContext::poll` | `src/game/net/pn_select.cpp` | **full jmp** |
| `0xD6EF90` | `PNConnectionNode::run_state_1` | `src/game/net/pn_connection.cpp` | **full jmp** |
| `0xD6C8E0` | `PNConnectionNode::run_state_2` | `src/game/net/pn_connection.cpp` | **full jmp** |
| `0xD6C9C0` | `PNConnectionNode::run_state_3` | `src/game/net/pn_connection.cpp` | **full jmp** |
| `0xD6E180` | `PNUpnpClient::AddPortMapping` | `src/game/net/pn_upnp.cpp` | **full jmp** (skip) |
| `0xD653B0` | `process_message_proudnet_layer` | `src/game/net/pn_process_message.cpp` | **full jmp** |
| `0xD65940` | drain receive queue | `src/game/net/pn_drain_recv.cpp` | **trace** (SEH `0xD65947`) |
| `0xD5DC10` / `0xD5CA30` | compress / encrypt | `src/hooks/net/pn_compress_hook.cpp` | **trace** passthrough |
| `0xD56220` / `0xD569C0` | `TCPSocket::Connect` / Send | `src/game/engine/TCPSocket.cpp` | **full jmp** / reimpl |
| `0x14314E0` / `0x014F6DC0` | `w_connect_2` / `w_connect_3` | `src/game/net/proud_connect.cpp` | **full jmp** |
| `0xD0C0A0` / `0xD0A340` | CNetClient factory / ctor | `src/hooks/net/pn_net_client_factory.cpp` | **trace** |

Hook registration: [`src/main.cpp`](../../src/main.cpp) (`HookManager::make_hook`). Layout constants: [`include/game/net/pn_layout.hpp`](../../include/game/net/pn_layout.hpp).

---

## WIP / disabled in `main.cpp`

| Area | RVA | Notes |
| --- | --- | --- |
| TCP recv framer | `0xD84BB0` | SEH prologue; resume `0xD84BB7`; `pn_tcp_frame_hook.cpp` |
| TCP send framer | `0xD84970` | Resume `0xD84977`; never hook `0xD84910` |
| FSM driver | `0xD6F7B0` | States 1–3 hooked; driver not |
| Per-case ProcessMessage handlers | `0xD64F10`, … | Tail-call GAME from reimpl |
| `Message_Read` / `CMessage::Read` | `0xD59300`, `0xD58B30` | Tail-call GAME from reimpl |

---

## Python dummy server

| Component | Path |
| --- | --- |
| TCP accept / threading | `server/server/serve.py` |
| ENTRY FSM | `server/server/entry_transport.py` |
| GAME FSM + replay | `server/server/game_transport.py` |
| Framing + RMI builders | `server/server/proud_frame.py`, `proud_rmi.py` |
| Replay data | `server/server/data/lobby_replay.json`, `lobby_postinit.json` |
| Wire tests | `server/server/test_wire_*.py` |

---

## Client headers (mirrors)

| Component | Path |
| --- | --- |
| Proud frame | `include/game/net/proud_frame.hpp` |
| Payload summary | `include/game/net/proud_message.hpp` |
| NetUserConnectRES | `include/game/net/net_user_connect_res.hpp` |

---

## Game RMI hooks (application layer)

See [../rmi/client.md](../rmi/client.md), [../rmi-ida-reimpl.md](../rmi-ida-reimpl.md), and [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md).

---

## Verify recipe

```powershell
just ensure-serve
just ctl::ping
just build-debug
just ctl::copy-dll
just ctl::launch-offline
just ctl::wait-stage shard_choice 240
just ctl::copy-logs
```

Inspect `ctl/logs/runs/<run_id>/events.jsonl` for faults and hook log prefixes.

*Last updated: 2026-05-29 - matches `src/main.cpp`; see hook-status plan for live matrix.*
