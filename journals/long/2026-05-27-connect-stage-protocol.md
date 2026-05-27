# Connect-stage protocol (long)

## IDA call graph (port 7000 path)

```
sub_D668B0  PNCliWorker thread
  sub_D70A10
    sub_D708F0
      sub_D6FF20   (per-task; switch on *(ctx+9392))
      sub_D6F7B0   (per-connection FSM)
        0: sub_D6EAC0  -> sub_D57A70 (bind), sub_D550C0 (nonblock), sub_D6D160 -> TCPSocket::Connect
        1: sub_D6EF90  -> select/sub_D55300, sub_D6C880 -> w_wsasend_1
        2: sub_D6C8E0  -> sub_D55660 (WSAGetOverlappedResult send), w_wsarecv_1(0x8000)
        3: sub_D6C9C0  -> sub_D55510 (recv complete), sub_D71CE0 (append), more recv
        4: sub_D6DC70  (copy buffer to String)
        5: terminal
```

## CFastSocket vs TCPSocket

| RVA | Symbol (IDA) | Role |
|-----|----------------|------|
| 0xD56220 | TCPSocket::Connect | Wide host + port; hooked |
| 0xD569C0 | w_wsasend_2 | TCPSocket::Send path (not used for PN worker send) |
| 0xD567F0 | w_wsasend_1 | **PN worker send** - hooked |
| 0xD56470 | w_wsarecv_1 | **PN worker recv** - hooked (passthrough) |
| 0xD71CE0 | sub_D71CE0 | Append recv bytes (7-byte prologue; not hooked yet) |

Socket handle at `this+0x12C` (300).

## onNetUserConnectRES (`sub_4BA070`)

`v2 = *(_DWORD *)(a1 + 8)` message payload:

- `*(WORD *)(v2 + 2)` - error; non-zero → `sub_675160` (codes include 30213/30214 auth-related)
- `*(DWORD *)(v2 + 4)`, `+8`, `+92`, `+96`, `+236`
- `wcscpy_s(..., v2 + 16)` - wide string (account/char?)

## Docker server

`server/entry_server.py` - accept loop, hex log, placeholder 256-byte reply (err=0 @ +2). Replace after first real TX capture in `game_netlogs.txt`.

## Commands

```text
docker compose -f server/docker-compose.yml up --build -d
just copy-dll
just run-session-offline
just copy-logs
```
