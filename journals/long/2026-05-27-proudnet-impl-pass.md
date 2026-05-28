# ProudNet implementation pass (2026-05-27)

## Subagents

- ProudNet sample: no :7000 wire format in SDK; game RE + stub server only.
- Latest run `009_96369131`: C2S `13 57 01 05 2f 0f 00 00 40`; no S2C in netlogs; `connecting_to_server` reached.
- IDA pass: NetUserConnectRES body **0x180** bytes; `account_id` at **+0xEC** (not +0x08); +0x08 is `session_uid`.

## Code

- `server/server/proud_rmi.py` - 0x180 body, session_uid/account_id split, name cap 37 wchars.
- `include/game/net/net_user_connect_res.hpp` - IDA field map.
- `include/game/net/proud_message.hpp` - inner opcode/RMI summarize.
- `src/hooks/socket/pn_recv_append.cpp` - fix __thiscall stack (was wrong fastcall + duplicate push); log RMI summary.

## WIP

- NOTIFY / NetConnectRES exact bodies from capture.
- `sub_8DCD70` char list gate.
- Confirm `7000 <` recv lines after append hook fix.

## Verify

```text
just build-debug
just ctl::copy-dll
just ctl::run-session-offline
```
