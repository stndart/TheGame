# pn_upnp full replacement - long journal

**Date:** 2026-05-27

## IDA Pro MCP

Tooling available (61 tools). `analyze_function` + full `py_eval` decompile for `0xD6E180`.

### Prototype

```c
void __thiscall sub_D6E180(struct in_addr *this)
```

IDA types `this` as `struct in_addr *` but offsets used (`+72`, `+76`, `+100`, `+88`, `+12`) indicate a larger UPnP/IGD client object; we treat it as opaque `PNUpnpClient`.

### Behaviour summary

1. Vector-construct 20 `String` fragments for SOAP template (`<?xml version="1.0"?>…AddPortMapping…`).
2. Fill `%d` slots from object fields (external/internal ports).
3. Pick TCP vs UDP from `this+76`.
4. Internal client IP via `sub_D6DD30`.
5. Concatenate SOAP body; build `POST %s HTTP/1.1…` with wide→multibyte host from parent object `v2+90`.
6. `sub_D72FC0` performs HTTP POST to IGD; stores response string at `this+12` (`*(this+3)` as String handle).

### Return value

**None** (`void`). Success and failure paths both unwind locals and return without `eax` semantics. Offline skip: empty return - matches original “give up / don’t block” behaviour from the old hook’s `ret`.

### Size / tail-call decision

1070 bytes, 59 basic blocks, SEH (`eh vector constructor/destructor`), many `String` refcount calls → tail-call original at **`0xD6E187`** (7-byte hook patch skip), not full reimpl.

### Callers

- `sub_D6EF90` @ `0xD6EC7A`, `0xD6F066` - PN connect FSM state 1.

## Hook pattern (matches TCPSocket)

```cpp
extern "C" void __declspec(naked) hook_pn_upnp() {
  __asm { jmp PNUpnpClient::AddPortMapping }
}
```

MSVC requires `__thiscall` on a **member** function (not namespace free function) so `ecx` stays the game object pointer from the caller.

## Verification run `023_ec65a5fb`

- DLL copied from `build/msvc-x86-debug/bin/TheGame.dll`
- `just ctl::launch-offline` (daemon briefly unavailable; launch still succeeded)
- `events.jsonl`: `connecting_to_server` line 97, `shard_choice` line 114 - fast progression (no multi-minute UPnP stall)
- `skip UPnP` log line not present in events/game_logs (same as prior runs with old trampoline hook); behaviour confirms skip or non-invocation on this path

## Collateral build fix

Parallel WIP `pn_select` full-replacement used invalid `__thiscall` on namespace function; fixed to `PNSelectContext::poll` member so `just build-debug` links. Unrelated to UPnP scope but blocked verification.

## Files touched

| File | Change |
|------|--------|
| `include/game/net/pn_upnp.hpp` | new |
| `src/game/net/pn_upnp.cpp` | new |
| `src/hooks/socket/pn_upnp_skip.cpp` | naked jmp only |
| `include/game/net/pn_select.hpp` | build unblock (class member) |
| `src/game/net/pn_select.cpp` | build unblock |
| `src/hooks/socket/pn_select_sync.cpp` | jmp target rename |
