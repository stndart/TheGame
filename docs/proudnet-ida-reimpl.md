# ProudNet - IDA / reimpl index

RVAs for **this debug build** of `GAME.exe`. Rebase in IDA if image base differs; hooks use the same addresses as `HookStub` in `src/main.cpp`. Layout constants: [`include/ProudNet/Layout.hpp`](../../include/ProudNet/Layout.hpp).

**Wire protocol:** [proudnet/protocol.md](proudnet/protocol.md). **Reimpl files:** [proudnet/implementation.md](proudnet/implementation.md). **Current hook matrix:** [plans/proudnet-hook-status.md](plans/proudnet-hook-status.md). **SDK crossmap (historical):** [proudnet-sdk-crossmap.md](proudnet-sdk-crossmap.md).

---

## Legend

| Column | Meaning |
| --- | --- |
| **RVA** | Address in `GAME.exe` (hook target when present). |
| **Hook** | `TheGame.dll` hook in `src/`. |
| **Mode** | **full jmp** - naked jmp to C++ reimpl. **trampoline** - mid-function patch. **trace** - log + tail-jump/resume. |

---

## PN class cluster

| Class | Key RVAs | Reimpl |
| --- | --- | --- |
| `PNGrowableBuffer` | `0x9FC720`, `0x9FCB70` | `pn_growable.cpp` |
| `PNRecvBuffer` | `0xD71CE0`, `0xD71610` | `pn_recv_append.cpp` |
| `PNConnectionNode` | `0xD6F7B0`, `0xD6C9C0` | `pn_connection.cpp` |
| `PNFastSocket` | `0xD567F0`, `0xD56470`, `0xD55510` | `pn_fast_socket.cpp`, `pn_fast_send.cpp` |
| `PNSelectContext` | `0xD55300` | `pn_select.cpp` |
| `PNUpnpClient` | `0xD6E180` | `pn_upnp.cpp` (skip) |

---

## Message dispatch core

| RVA | Role | Hook |
| --- | --- | --- |
| `0xD653B0` | `ProcessMessage_ProudNetLayer` | **full jmp** `pn_process_message.cpp` |
| `0xD65940` | Receive-queue drain | **trace** SEH resume `0xD65947` |
| `0xD59300` | `Message_Read` | tail-call from reimpl |
| `0xD58B30` | `CMessage::Read` | - |
| `0xD589C0` | Restore read offset | tail-call from reimpl |
| `0xD5FD30` | `IsFromRemoteClientPeer` | tail-call from reimpl |
| `0xD5DC10` / `0xD5CA30` | compress / encrypt unwrap | **trace** `pn_compress_hook.cpp` |
| `0xD366A0` | Alternate dispatch (`sub_D37BC0` batch) | - |

Per-case table: [proudnet/message-dispatch.md](proudnet/message-dispatch.md).

---

## 1. Proud TCP framing

| RVA | Role | Hook |
| --- | --- | --- |
| `0xD84BB0` | Recv frame parser (magic `0x5713`, varint) | **off** - WIP `pn_tcp_frame_hook.cpp` (resume `0xD84BB7`) |
| `0xD84970` | Send framer | **off** - resume `0xD84977` |
| `0xD85740` | Write 16-bit magic | - |
| `0xD59250` / `0xD59DB0` | Read / write varint length | - |

---

## 2. Connection FSM

```text
sub_D668B0   PNCliWorker thread
  sub_D70A10 → sub_D708F0 → sub_D6FF20 (task) → sub_D6F7B0 (states 0-5)
```

| RVA | State | Role | Hook |
| --- | --- | --- | --- |
| `0xD6F7B0` | - | FSM driver | - |
| `0xD6EAC0` | 0 | bind, connect | - |
| `0xD6EF90` | 1 | select, send | **full jmp** |
| `0xD6C8E0` | 2 | send complete, recv | **full jmp** |
| `0xD6C9C0` | 3 | recv complete, append | **full jmp** |
| `0xD6DC70` | 4 | copy buffer | - |
| `0xD6E180` | 1 pre-send | UPnP AddPortMapping | **full jmp** skip |
| `0xD6D160` | 0 | → TCPSocket::Connect | via TCPSocket hook |

### CNetClient construction

| RVA | Role | Hook |
| --- | --- | --- |
| `0xD0C0A0` | CNetClient factory | **trace** |
| `0xD0A340` | CNetClient ctor | **trace** |
| `0xD17000` | Manager singleton | - |
| `0xD66A30` | CNetClientManager ctor → worker thread | - |
| `0xD668B0` | PNCliWorker thread | - |

---

## 3. Socket I/O

Socket handle: **`this + 0x12C`** on fast-socket / worker objects.

| RVA | Role | Hook |
| --- | --- | --- |
| `0xD56220` | `TCPSocket::Connect` | **full jmp** `TCPSocket.cpp` |
| `0xD569C0` | `TCPSocket::Send` / WSASend | reimpl / trampoline |
| `0xD567F0` | `PNFastSocket::send` | **full jmp** |
| `0xD56470` | `PNFastSocket::recv` | **full jmp** |
| `0xD55510` | overlapped recv complete | **full jmp** |
| `0xD55300` | `select()` wrapper | **full jmp** |
| `0xD71CE0` | recv append | **full jmp** |
| `0xD56170` | report socket error | called from reimpl |
| `0x014F6DC0` | `w_connect_3` | **full jmp** |
| `0x14314E0` | `w_connect_2` | **full jmp** |

**Win32 IAT:** `connect` @ `0x01588BEC` - `ws32.cpp` `connect_syshandle`.

---

## 4. Game UI stages (ctl)

| RVA | Stage |
| --- | --- |
| `0x42A010` | `intro` |
| `0x42B280` | `login` |
| `0x4345B0` | `shard_choice` |
| `0x4347CC` | `server_ready` / main menu |

More stages: [plans/proudnet-rmi-server-plan.md](plans/proudnet-rmi-server-plan.md).

Game RMI RVAs: [rmi-ida-reimpl.md](rmi-ida-reimpl.md).

---

## 5. Connect-path hook index (`src/main.cpp`)

| RVA | Stub | Mode |
| --- | --- | --- |
| `0xD6E180` | `pn_upnp_skip.cpp` | **full jmp** |
| `0xD55300` | `pn_select_sync.cpp` | **full jmp** |
| `0xD71CE0` | `pn_recv_append.cpp` | **full jmp** |
| `0xD55510` | `pn_recv_complete.cpp` | **full jmp** |
| `0xD6C9C0` | `pn_connection_fsm.cpp` | **full jmp** |
| `0xD56470` | `proud_fast_socket.cpp` | **full jmp** |
| `0x014F6DC0` | `w_connect_3.cpp` | **full jmp** |
| `0x14314E0` | `w_connect_2.cpp` | **full jmp** |
| `0xD0C0A0` / `0xD0A340` | `pn_net_client_factory.cpp` | **trace** |
| `0xD567F0` | `send.cpp` `hook_send_2` | **full jmp** |
| `0xD56220` / `0xD569C0` | `TCPSocket.cpp` | **full jmp** / reimpl |
| `0xD653B0` | `pn_process_message_hook.cpp` | **full jmp** |
| `0xD65940` | `pn_drain_recv.cpp` | **trace** |
| `0xD5DC10` / `0xD5CA30` | `pn_compress_hook.cpp` | **trace** |

---

## 6. Engine `String` hooks (support)

| RVA | Member | File |
| --- | --- | --- |
| `0xCEFCC0`-`0xCF1DD0` | String ops | `src/hooks/str/string.cpp` |
| `0x5584C0` / `0x578BB0` | inline buf capacity | `str/inline_buf.cpp` |
| `0x535740` / `0x578C90` | converters | `str/converters.cpp` |

---

## 7. Transport opcodes (wire reference)

| Opcode | Name |
| --- | --- |
| `0x2F` | Hello |
| `0x04` | Key blob |
| `0x05` | Key response |
| `0x06` | ACK |
| `0x07` | Auth |
| `0x0A` | Redirect |
| `0x02` | RMI carrier / server info |
| `0x1B` / `0x1D` | Ping / pong |
| `0x1C` | Keepalive |
| `0x25` | Session |

Full spec: [proudnet/protocol.md](proudnet/protocol.md).

---

## Hooking discipline

1. Never tail-call `game_va(RVA + 5)` on MSVC **SEH prologue** functions.
2. Set **`k*Resume`** from IDA - first safe address after stolen prefix (often **+7**).
3. Enable hooks one at a time; verify with ctl recipe in [plans/proudnet-hook-status.md](plans/proudnet-hook-status.md).

*Last updated: 2026-05-29 - matches `src/main.cpp`.*
