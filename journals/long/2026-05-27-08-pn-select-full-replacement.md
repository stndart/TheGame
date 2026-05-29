# pn_select full replacement (long)

## IDA `sub_D55300` @ `0xD55300`

**Prototype:** `char __thiscall(fd_set *this, int optval, _DWORD *a3)` - decompiler misnames args; asm is authoritative.

**Prologue (7 bytes, old trampoline resume @ +7):**
```
push ecx; push esi; push edi
mov edi, [esp+0Ch+optval]   ; CFastSocket ctx
mov esi, ecx                ; fd-set bundle
```

**Logic:**
1. `sock = [edi+12Ch]` (CFastSocket+300)
2. If `__WSAFDIsSet(sock, bundle+104h)`: `*a3=0`, return 1
3. Else if `__WSAFDIsSet(sock, bundle+208h)`: `getsockopt(sock, 0xFFFF, 0x1007, &optval, &len=4)`; `*a3=optval`, return 1
4. Else return 0
5. `retn 8` (two stack args after ecx)

**Callers:** `sub_D6EF90` (PN worker select path), others.

## Sync insertion point

Trampoline called `sync_fast_socket_handle(optval)` **before** original prologue body - i.e. before first read of `[edi+12Ch]`. Full replacement calls sync at top of `PnSelect::poll` when offline override active.

## Guard semantics (`socket_trace.cpp`)

`sync_fast_socket_handle` returns early when slot peer is `:27380`, `:20009`, or already `:7000` - prevents ENTRY fd stomping GAME fd (documented in `2026-05-27-07-offline-proud-handshake` journal).

## Verification run `022_624ce004`

```
just ctl::copy-dll
just ctl::launch-offline
```

`events.jsonl` game_state sequence:
- `started`, `intro`, `connecting_to_server`
- connect logs: `127.0.0.1:7000`, then `127.0.0.1:27380`
- **`shard_choice`** @ line 114

`just ctl::wait-stage shard_choice 180` exited with daemon pipe timeout (~4s) while game was still progressing; stage already recorded in events.

## Files

| Path | Role |
|------|------|
| `include/game/net/pn_select.hpp` | Declaration |
| `src/game/net/pn_select.cpp` | Full reimpl |
| `src/hooks/socket/pn_select_sync.cpp` | Naked jmp hook @ `0xD55300` |
