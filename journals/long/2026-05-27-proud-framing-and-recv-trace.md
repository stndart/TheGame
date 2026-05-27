# Proud framing, recv trace, NetUserConnectRES (long)

## IDA — client expectations

### Transport (`sub_D84BB0` / `sub_D84970`)

- Magic LE **`0x5713`** → wire bytes **`13 57`**
- Length: varint via `sub_D9DEC0` (tag `1` + 1 byte if len≤127; tag `2` + WORD if larger)
- Payload: Proud message stream → type **`0x13`** RMI → dispatch by **`WORD` at message desc +40**

### NetUserConnectRES — `sub_4BA070`, RMI id **16190 (`0x3F3E`)**

Payload at `*(MsgDelegateArg+8)`:

| Offset | Type | Use |
|--------|------|-----|
| +2 | WORD | Error; 0=success; 30213/30214 → `sub_675160` |
| +4 | DWORD | Farm/select id |
| +8 | DWORD | Account id |
| +16 | WCHAR[260] | Name |
| +92, +96, +236 | DWORD | Misc |
| +0x16C | DWORD | Character slot count |

Success also needs `sub_8DCD70()` (local char list) and posts NOTIFY **16025** with word **15060**.

### PN FSM (`sub_D6F7B0`)

States 0–5: connect → send (`w_wsasend_1`) → recv (`w_wsarecv_1`, 32KiB) → append (`sub_D71CE0`) → copy to String.

`sub_D71CE0` call at `0xD6CA34`: `ecx=[conn+0x20]`, stack `(src, len)`.

### Other connect-path RES (registered in `sub_4B8630`)

| Msg ID | Handler |
|--------|---------|
| 16014 | `sub_4BA520` → ConnectRES-style (`sub_8DAEF0`) |
| 16017–16021, 16193–16194 | various `sub_4BA*` |

## Code changes

- `include/game/net/proud_frame.hpp`, `net_user_connect_res.hpp` — layouts
- `src/hooks/socket/pn_recv_append.cpp` — log RX at append (`7000 < hex`)
- `src/hooks/socket/send.cpp` — `logn` on `:7000` for `hook_send_2`
- `src/main.cpp` — `pn_recv_append`, `pn_select` enabled
- `server/server/proud_frame.py`, `serve.py` — wrap stub body in `13 57` frame

## WIP

- Full 48-byte Proud message header + RMI bitstream inside payload
- Mirror client TX from `game_netlogs.txt` for exact reply
- `sub_8DCD70` may block lobby even with err=0

## Live capture (run `000_609b1d15`)

**C2S** (9 B, `netlogs.txt` / server log):

```text
13 57 01 05  2f 0f 00 00 40
^magic  ^len5  ^payload
```

**S2C** (373 B): `13 57 02 70 01` + 0x170-byte NetUserConnectRES-shaped body (err=0 @ +2).

Client reached TCP :7000 and sent Proud-framed data; `connecting_to_server` stage not seen in `events.jsonl` (still `intro`). PN recv-append (`7000 <`) not seen — PN path may differ from `TCPSocket::Send` for this first packet.

## Verify

```text
just serve
just ctl::copy-dll
just ctl::launch-offline
just ctl::wait-stage connecting_to_server 180
just ctl::copy-logs
```
