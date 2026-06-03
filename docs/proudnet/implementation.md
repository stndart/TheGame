# ProudNet - implementation map

What is **reimplemented** in this repo for the ProudNet transport path and where the code lives. For RVAs see [../proudnet-ida-reimpl.md](../proudnet-ida-reimpl.md). For **installed** hooks see [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md).

**Layout:** trampolines in [`src/hooks/`](../../src/hooks/); bodies in [`src/ProudNet/`](../../src/ProudNet/). Registration: [`src/thegame/dll_main.cpp`](../../src/thegame/dll_main.cpp). Constants: [`include/ProudNet/Layout.hpp`](../../include/ProudNet/Layout.hpp).

---

## TheGame.dll - ProudNet API

| RVA | C++ method | Hook stub | Reimpl | Installed |
| --- | --- | --- | --- | --- |
| `0x9FC720` | `pn::growable::ensure_size` | — | `ProudNet/GrowableBuffer.cpp` | reimpl only |
| `0x9FCB70` | `pn::growable::calc_capacity` | — | `ProudNet/GrowableBuffer.cpp` | reimpl only |
| `0xD71610` | `PNRecvBuffer::ensure_capacity` | (via append) | `ProudNet/RecvAppend.cpp` | via append |
| `0xD71CE0` | `PNRecvBuffer::append` | `hooks/socket/pn_recv_append.cpp` | `ProudNet/RecvAppend.cpp` | yes |
| `0xD55510` | `PNFastSocket::recv_complete` | `hooks/socket/pn_recv_complete.cpp` | `ProudNet/FastSocket.cpp` | yes |
| `0xD55660` | `PNFastSocket::send_complete` | — | `ProudNet/FastSocket.cpp` | called from reimpl |
| `0xD56470` | `PNFastSocket::recv` | `hooks/socket/proud_fast_socket.cpp` | `ProudNet/FastSocket.cpp` | yes |
| `0xD567F0` | `PNFastSocket::send` | `hooks/socket/send.cpp` (`g_target_send_2`) | `ProudNet/FastSend.cpp` | yes |
| `0xD55300` | `PNSelectContext::poll` | `hooks/socket/pn_select_sync.cpp` | `ProudNet/SelectContext.cpp` | yes |
| `0xD6EF90`–`0xD6C9C0` | FSM states 1–3 | `hooks/socket/pn_connection_fsm.cpp` | `ProudNet/ConnectionNode.cpp` | yes |
| `0xD6E180` | `PNUpnpClient::AddPortMapping` | `hooks/socket/pn_upnp_skip.cpp` | `ProudNet/UpnpClient.cpp` | yes (skip) |
| `0xD653B0` | `process_message_proudnet_layer` | `hooks/net/pn_process_message_hook.cpp` | `ProudNet/ProcessProudNetLayer.cpp` | yes |
| `0xD65940` | drain receive queue | `hooks/net/pn_drain_recv_hook.cpp` | `ProudNet/DrainReceiveQueue.cpp` | yes (trace) |
| `0xD5DC10` / `0xD5CA30` | compress / encrypt | `hooks/net/pn_compress_hook.cpp` | — | yes (trace) |
| `0xD56220` | `TCPSocket::Connect` | `hooks/socket/tcpsocket.cpp` | `src/game/engine/TCPSocket.cpp` | yes |
| `0xD569C0` | `TCPSocket::Send` | `hooks/socket/tcpsocket.cpp` | — | **stub only** |
| `0x14314E0` / `0x014F6DC0` | `w_connect_2` / `w_connect_3` | `hooks/w_connect_*.cpp` | `src/game/net/proud_connect.cpp` | yes |
| `0xD0C0A0` / `0xD0A340` | CNetClient factory / ctor | `hooks/net/pn_net_client_factory.cpp` | — | yes (trace) |

---

## WIP / disabled (stub exists, not in default `dll_main`)

| Area | RVA | Notes |
| --- | --- | --- |
| TCP recv framer | `0xD84BB0` | `pn_tcp_frame_hook.cpp`; resume `0xD84BB7` |
| TCP send framer | `0xD84970` | resume `0xD84977`; never hook `0xD84910` |
| FSM driver | `0xD6F7B0` | States 1–3 hooked; driver not |
| Per-case ProcessMessage handlers | `0xD64F10`, … | Tail-call GAME from reimpl |
| `Message_Read` / `CMessage::Read` | `0xD59300`, `0xD58B30` | Tail-call GAME from reimpl |

Optional TCP frame install: [`src/ProudNet/TcpLayerMessageExtractor.cpp`](../../src/ProudNet/TcpLayerMessageExtractor.cpp).

---

## Friends dummy server (separate package)

> Not in this repo. **`ctl`** is also released standalone under `ctl/`. Paths below are reference for S2C integration with the friends server checkout.

| Component | Path |
| --- | --- |
| TCP accept / threading | `server/server/serve.py` |
| ENTRY FSM | `server/server/entry_transport.py` |
| GAME FSM + replay | `server/server/game_transport.py` |
| Framing + RMI builders | `server/server/proud_frame.py`, `proud_rmi.py` |
| Wire tests | `server/tests/test_wire_*.py` |

---

## Client headers (mirrors)

| Component | Path |
| --- | --- |
| Proud frame | `include/game/net/proud_frame.hpp` |
| Payload summary | `include/game/net/proud_message.hpp` |
| NetUserConnectRES | `include/game/net/net_user_connect_res.hpp` |

---

## Game RMI hooks (application layer)

See [../rmi/client.md](../rmi/client.md), [../rmi-ida-reimpl.md](../rmi-ida-reimpl.md), [../plans/proudnet-hook-status.md](../plans/proudnet-hook-status.md).

---

## Verify recipe

```powershell
just ensure-serve
just ctl::ping
just build
just ctl::copy-dll
just ctl::launch-offline
just ctl::wait-stage shard_select 240
just ctl::copy-logs
```

Inspect `ctl/logs/runs/<run_id>/events.jsonl` for faults and hook log prefixes.

*Last updated: 2026-06-03 — matches `dll_main.cpp`.*
