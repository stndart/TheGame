# GAME.exe symbol map (ProudNet / offline connect path)

RVAs are for **this debug build** of `GAME.exe`. Rebase in IDA if your image base differs; hooks use the same numeric addresses as in `HookStub` definitions.

**ProudNet SDK (`ProudNet/`):** Bundled tree is **~1.8 sample**; the game ships **~1.7**-era binaries. A search for magic `0x5713`, `CFastSocket`, and `NetUserConnect` in the SDK tree did **not** yield symbols at these RVAs. Treat SDK names below as **conceptual analogues** only (e.g. `CNetClient::Connect` in `Sample/Simple/Client/Client.cpp`), not proof of mangled names in `GAME.exe`.

**Wire protocol:** Field layouts for `0x3F3E` and framing are in [proudnet-offline-protocol.md](proudnet-offline-protocol.md).

**Object layout / RVAs:** [`pn_layout.hpp`](../include/game/net/pn_layout.hpp) (offsets + `pn::rva::*`). Reimpls: [`pn_growable`](../include/game/net/pn_growable.hpp), [`pn_fast_socket`](../include/game/net/pn_fast_socket.hpp), [`pn_connection`](../include/game/net/pn_connection.hpp).

---

## Legend

| Column | Meaning |
|--------|---------|
| **RVA** | Address in `GAME.exe` (hook target when present). |
| **IDA / inferred name** | Name from IDA or assembly comment in this repo. |
| **Role** | What the code does in the offline connect path. |
| **Hook** | `TheGame.dll` hook in `src/` (file). |
| **Mode** | **full jmp** - naked `jmp` to C++ reimpl (original body not run). **trampoline** - mid-function patch, handler then `jmp return_addr`. **trace** - trampoline, log only. |
| **ProudNet SDK** | Closest public API in bundled SDK, if any. |

### PN class cluster (`pn_layout.hpp`)

| Class | Key RVAs | Role |
|-------|----------|------|
| `PNGrowableBuffer` | `0x9FC720`, `0x9FCB70` | `pn::growable::ensure_size`, `calc_capacity` (`pn_growable.cpp`). |
| `PNRecvBuffer` | `0xD71CE0`, `0xD71610` | `append` / `ensure_capacity` @ `conn+0x20` (`pn_recv_append.cpp`). |
| `PNConnectionNode` | `0xD6F7B0`, `0xD6C9C0` | FSM driver + `run_state_3` trace wrapper hook (`pn_connection.cpp`). |
| `PNFastSocket` | `0xD567F0`, `0xD56470`, `0xD55510` | `send` (**full jmp**), recv (orig), `recv_complete` (**full jmp** @ `0xD55510`; `pn_fast_socket.cpp`). |
| `PNSelectContext` | `0xD55300` | `select()` fd-set bundle; syncs socket @ fast socket `+0x12C`. |
| `PNUpnpClient` | `0xD6E180` | UPnP `AddPortMapping` (offline skip). |

### C++ API (`RVA` → method)

| RVA | C++ method | File |
|-----|------------|------|
| `0x9FC720` | `pn::growable::ensure_size` | `pn_growable.cpp` |
| `0x9FCB70` | `pn::growable::calc_capacity` | `pn_growable.cpp` |
| `0xD71610` | `PNRecvBuffer::ensure_capacity` | `pn_recv_append.cpp` |
| `0xD71CE0` | `PNRecvBuffer::append` | `pn_recv_append.cpp` |
| `0xD55510` | `PNFastSocket::recv_complete` | `pn_fast_socket.cpp` |
| `0xD55660` | `PNFastSocket::send_complete` | `pn_fast_socket.cpp` |
| `0xD558A0` | `PNFastSocket::recv_staging_ptr` | `pn_fast_socket.cpp` |
| `0xD56170` | `PNFastSocket::report_error` | `pn_fast_socket.cpp` |
| `0xD56470` | `PNFastSocket::recv` | `pn_fast_socket.cpp` |
| `0xD567F0` | `PNFastSocket::send` | `pn_fast_socket.cpp` |
| `0xD6C880` | `PNSendArm::arm_send` | `pn_connection.cpp` |
| `0xD551E0`–`0xD552A0` | `pn::select_fd::{init,add_*,wait}` | `pn_select_fd.cpp` |
| `0xD6EF90` | `PNConnectionNode::run_state_1` | `pn_connection.cpp` (**full jmp**) |
| `0xD6C8E0` | `PNConnectionNode::run_state_2` | `pn_connection.cpp` (**full jmp**) |
| `0xD6E5B0` | UPnP delete (state 1 tail) | `pn_connection.cpp` |
| `0xD6C9C0` | `PNConnectionNode::run_state_3` | `pn_connection.cpp` (**full jmp**, `__stdcall` hook) |
| `0xCF0460` | `PNFastSocket::addr_port_update` | in `recv_complete` (`kAddrPort` @ +0xE4) |

---

## 1. Proud TCP framing (all `:7000` / `:27380` traffic)

| RVA | IDA / inferred name | Role | Hook | ProudNet SDK |
|-----|---------------------|------|------|----------------|
| `0xD84BB0` | `sub_D84BB0` | **Recv-side frame parser** - validates magic `0x5713`, reads varint length (`sub_D59250`), reads payload (`sub_D58D80`). | - | No symbol match; conceptually “message stream read” |
| `0xD84970` | `sub_D84970` | **Send-side framer** - writes magic (`sub_D85740`), payload size (`sub_D59DB0`), payload bytes. | - | Same |
| `0xD85740` | `sub_D85740` | Write 16-bit magic to stream. | - | - |
| `0xD59250` | `sub_D59250` | Read varint length (1/2/4-byte tag). | - | - |
| `0xD59DB0` | `sub_D59DB0` | Write varint length. | - | - |
| `0xD58D30` / `0xD58D80` | stream helpers | Buffer read/write around parser. | - | - |

**Repo mirror:** `include/game/net/proud_frame.hpp`, `server/server/proud_frame.py`.

---

## 2. ProudNet client worker FSM (per TCP connection)

Call graph (ENTRY `:7000` path) from IDA notes:

```text
sub_D668B0   PNCliWorker thread
  sub_D70A10
    sub_D708F0
      sub_D6FF20     task dispatcher (*(ctx+9392))
      sub_D6F7B0     per-connection FSM (states 0–5)
```

| RVA | IDA / inferred name | FSM state | Role | Hook | ProudNet SDK |
|-----|---------------------|-----------|------|------|----------------|
| `0xD6F7B0` | `sub_D6F7B0` | - | Connection state machine driver. | - | - |
| `0xD6EAC0` | `sub_D6EAC0` | 0 | Setup: bind (`sub_D57A70`), nonblock (`sub_D550C0`), `sub_D6D160` → **TCPSocket::Connect**. | - | - |
| `0xD6EF90` | `sub_D6EF90` | 1 | `select` / `sub_D55300`, then `sub_D6C880` → **send**. | - | - |
| `0xD6C8E0` | `sub_D6C8E0` | 2 | Overlapped send complete (`sub_D55660`), **recv** 32 KiB. | - | - |
| `0xD6C9C0` | `sub_D6C9C0` | 3 | Recv complete, **append**, re-arm recv. | `pn_connection_fsm.cpp` (**full jmp**) → `PNConnectionNode::run_state_3` | - |
| `0xD6DC70` | `sub_D6DC70` | 4 | Copy accumulated buffer to `String`. | - | - |
| `0xD6E180` | `sub_D6E180` | 1 (pre-send) | **UPnP AddPortMapping** SOAP; blocks offline if not skipped. | `pn_upnp_skip.cpp` (**full jmp**) | - |
| `0xD6D160` | `sub_D6D160` | 0 | Calls into **TCPSocket::Connect** for hostname/port. | via `TCPSocket::Connect` | Analogous to `CNetClient::Connect` + worker |

---

## 3. Socket I/O (CFastSocket / TCPSocket)

**Socket handle offset:** `this + 0x12C` (300 decimal) on fast-socket / worker objects (also used by `SocketTrace`).

| RVA | IDA / inferred name | Role | Hook | ProudNet SDK |
|-----|---------------------|------|------|----------------|
| `0xD56220` | `TCPSocket::Connect` | Wide hostname + port; DNS/`connect()`; **ctl stage** `connecting_to_server` when port `7000`. | `TCPSocket.cpp` (reimpl) | - |
| `0xD569C0` | `TCPSocket::Send` / `w_wsasend_2` | Overlapped **WSASend** for `TCPSocket` message buffers; **used for Proud hello on offline path**. | `TCPSocket.cpp` + `tcpsocket.cpp` trampoline | - |
| `0xD567F0` | `w_wsasend_1` | **PN worker fast send** (`PNFastSocket::send`). | `send.cpp` `hook_send_2` → `pn_fast_send.cpp` (**full jmp**) | - |
| `0xD56470` | `w_wsarecv_1` | **PN worker recv** (overlapped). | `proud_fast_socket.cpp` (**full jmp**) → `PNFastSocket::recv` | - |
| `0xD55510` | `sub_D55510` | **Overlapped recv complete** (`WSAGetOverlappedResult` path; FSM state 3). | `pn_recv_complete.cpp` (**full jmp**) → `PNFastSocket::recv_complete` | - |
| `0xD55300` | `sub_D55300` | `select()` wrapper; `optval` = CFastSocket ctx; syncs socket into `this+300`. | `pn_select_sync.cpp` (**full jmp**) | - |
| `0xD71CE0` | `sub_D71CE0` | Append completed recv bytes to PN buffer (FSM state 3). | `pn_recv_append.cpp` (**full jmp**) | - |
| `0xD71610` | `sub_D71610` | Recv accumulator growth (`PNRecvBuffer::ensure_capacity`). | in-hook @ `0xD71CE0` only (`pn_recv_append.cpp`; no separate hook) | - |
| `0xD71060` | `sub_D71060` | Capacity slack for `sub_D71610`. | reimpl as `calc_capacity` | - |
| `0xD56170` | `sub_D56170` | Report socket error to callback (skips WOULDBLOCK / IO_PENDING). | called from `TCPSocket.cpp` | - |
| `0xCEFB40` | `sub_CEFB40` | Fatal “out of range” formatter for bad send args. | called from `TCPSocket.cpp` | - |
| `0x014F6DC0` | `w_connect_3` | ProudNet internal **`connect(sockaddr_in)`** (`this+4` sockaddr, `this+20` socket). | `w_connect_3.cpp` (**full jmp**) | - |
| `0x14314E0` | `w_connect_2` | Alternate connect helper (~591 B); DNS/bind/nonblock/`connect`. | `w_connect_2.cpp` (**full jmp**) → `ProudConnect::w_connect_2` (`proud_connect.cpp`) | - |

**Win32 IAT (`GAME.exe`):** `connect` @ `0x01588BEC` (+ `TheGame.dll` import) - `ws32.cpp` `connect_syshandle` (remap + track).

**Legacy / unused send RVAs (not hooked):**

| RVA | Name | Notes |
|-----|------|-------|
| `0xCF3290` | `hook_send_1` target | Generic send path; removed from DLL. |
| `0xD57590` / `0xD577B0` / `0xD57850` | `sendto` family | UDP; hooks removed from DLL. |

---

## 4. RMI registration and connect handlers

| RVA | IDA / inferred name | Role | Hook | ProudNet SDK |
|-----|---------------------|------|------|----------------|
| `0x4B8630` | `sub_4B8630` | **RMI dispatch table setup** - registers handlers by id (`sub_556CB0`). | - | Generated stub/proxy tables in samples |
| `0x4BA070` | **`onNetUserConnectRES`** / `sub_4BA070` | Handles RMI **`0x3F3E`** (16190); reads 384 B body; sets account/session/farm fields. | - (server sends body) | Custom game RMI, not in Simple sample |
| `0x65EAA0` | `sub_65EAA0` | Alternate / extended handler path for same message family. | - | - |
| `0x4BA520` | `sub_4BA520` | Handler for RMI **`0x3E8E`** (16014) NetConnectRES-style. | - | - |
| `0x890AA0` | `sub_890AA0` | Consumes **`session_uid`** from `+0x08` of connect body. | - | - |
| `0x416830` | `sub_416830` | Farm / shard selector; uses **`field_5c`**. | - | - |
| `0x415440` | `sub_415440` | Called with pointer into body `+0x70`. | - | - |
| `0x675160` | `sub_675160` | Disconnect / error path when `error_code != 0`. | - | - |
| `0x8DCD70` | `sub_8DCD70` | **Local character list** gate (WIP - may block lobby). | - | - |
| `0x401720` | `sub_401720` | Singleton getter noise in connect handler prologue. | - | - |

**Repo struct map:** `include/game/net/net_user_connect_res.hpp` ↔ `sub_4BA070` field offsets.

---

## 5. Game UI stages (ctl diagnostics)

| RVA | IDA / inferred name | Role | Hook |
|-----|---------------------|------|------|
| `0x42A010` | `CGameIntro::onPreProcess` | Stage **`intro`**. | `game_state.cpp` |
| `0x42B280` | `CGameLogin::onPreProcess` | Stage **`login`**. | `game_state.cpp` |
| `0x4345B0` | `CGameServer::onPreProcess` **Begin** | Stage **`shard_choice`**. | `game_state.cpp` |
| `0x4347CC` | `CGameServer::onPreProcess` **End** | Stage **`main_menu`**. | `game_state.cpp` |

---

## 6. DLL entry

| RVA | Name | Role | Hook |
|-----|------|------|------|
| `0x014AB273` | `GAME.exe` entry | Trampoline to original startup. | `entrypoint.cpp` |

---

## 7. Engine `String` hooks (support reimplemented `TCPSocket`)

Not on the hot connect path; hooked so reimplemented code can call original `EE::String` routines.

| RVA | Inferred member | Hook file |
|-----|-----------------|-----------|
| `0xCEFCC0` | `String::Truncate` | `str/string.cpp` |
| `0xD5A7E0` | `String::TruncateSelf` | |
| `0x9FCAF0` | `String::DecRefCount` | |
| `0xCEFDF0` | `String::CopyOnWrite` | |
| `0xCEFEF0` | `String::Realloc` | |
| `0xCF0020` | `String::Reserve` | |
| `0xCF2D90` | `TruncateAtFirstOccurrence` | |
| `0xCF2CD0` | `TrimLeft` | |
| `0xCF1A90` | `Concatenate` | |
| `0xCF1BB0` | `Vformat` (instance) | |
| `0xCF1DD0` | `vformat` (static) | |
| `0x5584C0` | `EnsureMStringBufferCapacity` | `str/inline_buf.cpp` |
| `0x578BB0` | `EnsureWStringBufferCapacity` | |
| `0x535740` | `ConvertWideToMultiByte` | `str/converters.cpp` |
| `0x578C90` | `ConvertMultiByteToWide` | |

---

## 8. Connect-path hook index (`src/main.cpp`)

ProudNet / connect hooks registered in `DllMain` (not generic Win32 IAT except `connect`).

| RVA | Symbol | Hook stub | Mode | Reimpl / behavior |
|-----|--------|-----------|------|-------------------|
| `0xD6E180` | `sub_D6E180` | `pn_upnp_skip.cpp` | **full jmp** | `PNUpnpClient::AddPortMapping` - offline skip inside body (`src/game/net/pn_upnp.cpp`). |
| `0xD55300` | `sub_D55300` | `pn_select_sync.cpp` | **full jmp** | `PNSelectContext::poll` - full `select` post-process + guarded sync @ CFastSocket+300 (`pn_select.cpp`). |
| `0xD71CE0` | `sub_D71CE0` | `pn_recv_append.cpp` | **full jmp** | `PNRecvBuffer::append` + in-hook `ensure_capacity` (replaces `sub_D71610` / `sub_D71060`; no hook @ `0xD71610`); S2C hex via `SocketTrace`. `fmt_error_x2` / OOM throw tail-call game @ `0xCEFAE0`, `0xCEF7C0`, `0x14AB3BC`. |
| `0xD55510` | `sub_D55510` | `pn_recv_complete.cpp` | **full jmp** | `PNFastSocket::recv_complete` - reimpl overlapped RX complete + log (`pn::rva::kRecvComplete`). |
| `0xD6C9C0` | `sub_D6C9C0` | `pn_connection_fsm.cpp` | **full jmp** | `PNConnectionNode::run_state_3` - reimpl FSM state 3 (`__stdcall` shim); uses `recv_complete` / `append` / `recv`. |
| `0xD56470` | `w_wsarecv_1` | `proud_fast_socket.cpp` | **full jmp** | `PNFastSocket::recv` reimpl. |
| `0x014F6DC0` | `w_connect_3` | `w_connect_3.cpp` | **full jmp** | `ProudConnect::Socket::w_connect_3` - remap + `connect_syshandle` (`proud_connect.cpp`). |
| `0x14314E0` | `w_connect_2` | `w_connect_2.cpp` | **full jmp** | `ProudConnect::w_connect_2` - full `sub_14314E0` reimpl; remap via `connect_syshandle` on connect leg. |

**Related (same path, different mode):**

| RVA | Hook | Mode | Notes |
|-----|------|------|-------|
| `0xD567F0` | `send.cpp` `hook_send_2` | **full jmp** | `PNFastSocket::send` reimpl (`pn_fast_send.cpp`); PN worker TX hex via `SocketTrace`. |
| `0xD56220` / `0xD569C0` | `TCPSocket.cpp` | **full jmp** / reimpl | Primary `:7000` / `:27380` connect + send. |
| `0x01588BEC` | `ws32.cpp` `connect_syshandle` | IAT + syshook | Remap + track all `connect()` entry points. |

---

## 9. Offline-path hook summary (what the DLL actually needs)

| Priority | Hook | Why |
|----------|------|-----|
| Required | `TCPSocket::Connect` / Send | Remap, stage emit, send hex on PN ports |
| Required | `w_connect_3` + `w_connect_2` + `connect` IAT | Both connect helpers **full jmp**; alternate ProudNet bind/connect path |
| Required | `pn_select` | **full jmp** - prevent ENTRY fd overwriting GAME fd @ `+300` |
| Required | `pn_upnp` | **full jmp** - avoid UPnP stall offline |
| Useful | `pn_recv_append` | **full jmp** - S2C hex in `game_netlogs.txt` |
| Useful | `pn_recv_complete` | **full jmp** - `PNFastSocket::recv_complete` reimpl + RX byte log |
*Last updated: 2026-05-28 -- hooks: `w_connect_2` / `w_connect_3` (**full jmp**), `pn_recv_complete`, `pn_recv_append`, `pn_select`, `pn_upnp`, `hook_send_2`. Trace hooks removed. Matches `src/main.cpp`.*
| Useful | `w_wsasend_1` (`send_2`) | **full jmp** - `PNFastSocket::send` + PN worker TX hex |
| Useful | Game state hooks | ctl `wait-stage` |
| Off by default | `send_1`, `sendto_*` | Trace-only jmp to original; commented out in `main.cpp` (not needed offline). |
| Optional | ws32 send syshooks | Installed; `WS32_SYSLOGS=0` by default |
| Disabled in main | `send_3`, `fast_wsasend`/`fast_wsarecv`, `recv_*` | Same RVA as `TCPSocket::Send` / `send_2` or WIP |

---

## 10. Transport opcodes (wire, not RVAs)

Cross-reference only - see protocol doc.

| Opcode | Name |
|--------|------|
| `0x2F` | Hello |
| `0x04` | Key blob |
| `0x05` | Key response |
| `0x06` | ACK |
| `0x07` | Auth |
| `0x0A` | Redirect |
| `0x02` | Server info or RMI wrapper |
| `0x1B` / `0x1D` | Ping / pong (GAME leg) |
| `0x1C` | Keepalive |
| `0x25` | Session |

---

## RVA elimination status

**DONE** (reimplemented; no tail-jmp to GAME for these paths):

- FSM states 1-3 (`pn_connection_fsm` -- state 3 **full jmp**; trace wrapper **deleted**)
- Fast send/recv (`pn_fast_socket`, `hook_send_2` / `w_wsasend_1`)
- `pn_recv_complete`, `pn_recv_append`, `pn_select`, `pn_upnp`
- `TCPSocket::Connect`, **`w_connect_2`**, `w_connect_3` (full jmp; `proud_connect.cpp` + `game_exe::fn` helpers)
- Growable heap (`pn_growable`)
- `socket_report_error` (no vtable)
- **`game_exe.hpp` / `rva::` call sites** in connect reimpl (typed tail-calls only)

**DELETED** (removed from tree / `main.cpp`):

- FSM state 3 **trace** hook (`pn_fsm_state3` log-only trampoline)
- Disabled **trace** hooks in `send.cpp` / `sendto.cpp` (were off in `main.cpp`; stubs removed)

**REMAINING** (acceptable):

- `OnWarning` vtable (keep original callback)
- Hook-install RVAs in `pn_layout.hpp` / `HookStub` targets only (metadata, not tail-jmp in reimpl bodies)

**Category B** (`src/game/net/`, `src/hooks/socket/`, `TCPSocket.cpp`): expect **0** hits for `return_addr`, raw GAME-RVA `reinterpret_cast`, `push 0x015`, `OnWarning`, `g_*_orig`; `game_exe::fn` is intentional (DONE above).

---

*Last updated: 2026-05-28 -- `w_connect_2` DONE, `game_exe` helpers DONE, trace hooks DELETED. Matches `src/main.cpp`.*
