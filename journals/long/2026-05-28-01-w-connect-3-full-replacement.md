# w_connect_3 full replacement (long)

## IDA @ 0x014F6DC0

Raw bytes (27 B):

```
6A 10                push 16
8D 41 04             lea eax, [ecx+4]
8B 49 14             mov ecx, [ecx+14h]   ; SOCKET at this+20
50                   push eax
51                   push ecx
FF 15 EC8B5801       call connect
33 D2                xor edx, edx
83 F8 FF             cmp eax, -1
0F 95 C2             setne dl
8A C2                mov al, dl
C3                   ret
```

Decompile:

```c
bool __thiscall w_connect_3(int this) {
  return connect(*(_DWORD *)(this + 20), (sockaddr *)(this + 4), 16) != -1;
}
```

Xrefs: single data ref at `0x16af594` (vtable slot).

## Overlap with w_connect_2

| Symbol | RVA | Role |
|--------|-----|------|
| `w_connect_3` | `0x014F6DC0` | 27 B standalone; vtable `connect(sockaddr_in@+4)` |
| `w_connect_2` | `0x014314E0` | ~591 B alternate path; hook patches inner `connect` @ `0x014316A2` |

No code overlap (different RVAs). Offline run `026_350684ab` reached `shard_choice`; `:27380` went through `TCPSocket::Connect` (game_logs), not `w_connect_3:27380`. Both hooks can remain until a trace shows only one path is live.

## ctl run 026_350684ab

- `events.jsonl`: `started` → `intro` → `connecting_to_server` → `shard_choice`
- `game_logs.txt`: `TCPSocket::Connect remap` / `TCPSocket:27380 target 127.0.0.1:27380`
- No crash on naked `jmp ProudConnect::Socket::w_connect_3`
