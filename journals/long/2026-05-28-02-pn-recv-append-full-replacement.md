# pn_recv_append full replacement (long)

## IDA - sub_D71CE0 @ 0xD71CE0

Prototype: `void __thiscall(_DWORD *this, const char *src, int len)`  
Size: 81 bytes, CC=5.

Pseudocode:

```c
void append(_DWORD *this, const char *src, int len) {
  if (len < 0) fmt_error_x2();
  if (!len) return;
  int old = this[3];           // +0x0C size
  sub_D71610(this, old + len); // grow / set logical size
  char *base = this[3] ? (char *)this[2] : NULL;  // +0x08 data
  for (int i = 0; i < len; ++i)
    base[old + i] = src[i];
}
```

Callees: `sub_D71610` (ensure capacity + resize), `fmt_error_x2` @ `0xCEFAE0`.

Assembly tail: `retn 8` (two stack args after `this` in ecx).

## Caller - sub_D6C9C0 (FSM state 3 recv complete)

After `sub_D55510` (WSAGetOverlappedResult):

- `len` = cbTransfer in local struct (`v12`)
- `fast_socket` = `*(* (conn+8))` when present
- `src` = `sub_D558A0(fast_socket)` - staging buffer ptr (+0x94 when flag +0x98 set)
- `this` for append = **`conn+0x20`** (accumulator), **not** fast socket

## sub_D71610

Left as explicit function pointer @ `0xD71610` (allocator / heap growth). Full reimpl would need `w_heap_alloc`, Proud allocator vtable, etc.

## Reimpl

`PNRecvBuffer::append` mirrors the loop; logging uses `SocketTrace::note_fast_socket_ctx` from `PNSelectContext::poll` so recv hex keys off CFastSocket+0x12C, not the accumulator `this`.

## Hook

```asm
hook_pn_recv_append:
  jmp PNRecvBuffer::append
```

`HookStub.return_rva = 0` - no trampoline.

## Verify - run 028_62449749

- Build: `just build-debug` OK
- Launch offline after `ctl::copy-dll`
- `events.jsonl`: `intro` → `connecting_to_server` → **`shard_choice`**
- `wait-stage shard_choice` failed with daemon pipe timeout (~10s); stage event present in log anyway

## Files

| File | Role |
|------|------|
| `include/game/net/pn_recv_append.hpp` | Class decl |
| `src/game/net/pn_recv_append.cpp` | Reimpl + log |
| `src/hooks/socket/pn_recv_append.cpp` | Naked jmp |
| `src/game/net/socket_trace.cpp` | `note_fast_socket_ctx` |
| `src/game/net/pn_select.cpp` | Records fast ctx on poll |
