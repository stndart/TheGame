# Long journal: NetUserConnectRES body struct layout

## IDA sessions

### sub_D84BB0 @ 0xD84BB0 - TCP recv / frame parser

```
d84c50  push 2              ; read 2 bytes
d84c5b  call sub_D58D30
d84c68  mov edx, 5713h
d84c71  cmp word ptr [esp+var_78], dx   ; == 0x5713 (bytes 13 57)?
d84c76  jz  loc_D84CD8                   ; yes → continue
; else: scan for next magic byte (EoF fallback)

d84cdd  call sub_D59250     ; read VarInt payload length into [esp+var_70]
d84cea  mov ebp, [esp+var_70]   ; v9 = length
d84cf0  jl  error            ; negative = invalid
d84cfd  jg  error            ; > max = too large
d84d04  call sub_D58D80      ; read `ebp` payload bytes into local buffer
```

Frame format confirmed: `[13 57] [VarInt len] [payload]`

VarInt from sub_D59250 / sub_D59DB0 uses the tag scheme already in proud_frame.hpp
(tag 1 → 1B length; tag 2 → 2B LE; tag 4 → 4B LE).

### sub_D84970 @ 0xD84970 - send path

```
d849a2  push 5713h
d849a9  call sub_D85740     ; write magic 0x5713 to outgoing stream
d849b7  call sub_D749F0     ; get payload size
d849c1  call sub_D59DB0     ; write size as VarInt (64-bit input → same tag scheme)
```

Symmetric to recv, confirms framing.

### sub_D85740 - write magic

`sub_D85740(stream, byte_arg)` - the decompiler says arg = 19 decimal but asm shows
`push 5713h` before the call; it writes the two-byte magic to the stream buffer.
The decompiler confused two separate pushes.

### sub_4B8630 @ 0x4B8630 - RMI dispatch (contains NetUserConnectRES handler registration)

```
4b865c  mov eax, offset sub_4BA070   ; handler fn pointer
4b866d  mov [eax], edx               ; [frame+0] = handler ptr
4b866f  mov [eax+4], ecx             ; [frame+4] = 0
4b8676  mov [eax+8], ebx             ; [frame+8] = this - 0x5C (context)
4b867f  mov [eax+0Ch], ecx
4b8682  call sub_556CB0              ; register handler
...
4b86a1  call edx                     ; vtable[1]: pop next RMI msg into local struct
4b86a3  cmp word ptr [esi+0Ch], 3F3Eh   ; esi = parsed RmiMsg, [+0xC] = RMI ID
4b86a9  jnz ...                         ; skip if not NetUserConnectRES
```

The RmiMsg descriptor (48-byte queue element, stride confirmed in sub_D85800) has the RMI ID
at offset +0xC as a uint16_t. After the vtable pop, the body pointer is at descriptor+8,
which becomes `a1` to sub_4BA070; the handler reads `*(a1+8)` for the actual body.

### sub_4BA070 @ 0x4BA070 - "onNetUserConnectRES Begin"

Full assembly, key field reads:

```asm
; a1 = RmiMsg descriptor (48 bytes)
4ba09c  mov esi, [esi+8]          ; v2 = body ptr = *(a1+8)
4ba09f  cmp word ptr [esi+2], 0   ; error_code @ +0x02
4ba0a8  jz  success_path

; success_path:
4ba0e8  mov eax, [esi+0ECh]       ; account_id @ +0xEC
4ba0ee  mov [ecx+88h], eax        ; → netmgr+136
4ba0f4  mov [ecx+8Ch], eax        ; → netmgr+140

4ba14a  mov eax, [esi+8]          ; session_uid @ +0x08
4ba14d  push eax                  ; arg to sub_890AA0 (via remaining stack after sub_401720 call)
4ba14e  call sub_401720            ; singleton getter, ignores push
4ba153  push eax                  ; push singleton
4ba154  call sub_890AA0            ; sub_890AA0(singleton, session_uid)

4ba159  call sub_401720
4ba15e  lea edi, [esi+10h]        ; name @ +0x10
4ba16b  call wcscpy_s              ; wcscpy_s(singleton+12, 260, v2+0x10)

4ba185  mov ecx, [esi+4]          ; farm_select_id @ +0x04
4ba188  mov [eax+24h], ecx        ; → outgamemgr+36

4ba19e  lea eax, [esi+70h]        ; unk_block @ +0x70 → sub_415440
4ba26f  mov eax, [esi+5Ch]        ; field_5c @ +0x5C → sub_416830
4ba266  mov edx, [esi+60h]        ; field_60 @ +0x60 → player[1055]

; loop: 3 farm slots at +0xF0, stride 40 bytes:
4ba2a7  add esi, 0F0h
4ba2ec  mov ecx, 28h              ; stride = 40
4ba2f1  add [...], ecx            ; esi += 40 per iteration (3 iterations)

4ba312  mov eax, [edx+16Ch]       ; num_char_slots @ +0x16C
```

### sub_65EAA0 @ 0x65EAA0 - alternate handler (registered via sub_660550)

Shares the same body pointer protocol. Additional reads:

```asm
65eb26  mov edx, [ebx+0E0h]       ; +0xE0 → obj+0x278
65eb32  mov eax, [ebx+0E4h]       ; +0xE4 → obj+0x27C
65ebb2  lea eax, [ebx+17Ch]       ; sub_923E30 reads from +0x17C
65ebc8  mov ebx, [ebx+0ECh]       ; account_id @ +0xEC (same as sub_4BA070)
```

Confirms: total struct reaches 0x17C+4 = 0x180.

### sub_401720 @ 0x401720

Singleton pattern - ignores all pushed args, returns global `dword_1C1530C`.
The `push eax` before each call is actually the argument to the NEXT function that
receives two items off the stack.

---

## Derived C struct

```c
// RMI body for NetUserConnectRES (ID 0x3F3E), wire = raw bytes, no extra framing
#pragma pack(push, 1)
struct NetUserConnectRES_Body {
    uint16_t pad_00;           // +0x00: ignored
    uint16_t error_code;       // +0x02: 0=ok, non-0 → disconnect
    uint32_t farm_select_id;   // +0x04: outgamemgr+36
    uint32_t session_uid;      // +0x08: to sub_890AA0 (not the account_id!)
    uint32_t pad_0c;           // +0x0C: unknown
    wchar_t  name[38];         // +0x10..+0x5B: display name (76 bytes)
    uint32_t field_5c;         // +0x5C: sub_416830 input
    uint32_t field_60;         // +0x60: player.slot_table[1055]
    uint8_t  pad_64[12];       // +0x64..+0x6F: unknown
    uint8_t  unk_block[0x70];  // +0x70..+0xDF: passed to sub_415440; zeroes safe
    uint32_t field_e0;         // +0xE0: obj+0x278 (sub_65EAA0 path)
    uint32_t field_e4;         // +0xE4: obj+0x27C (sub_65EAA0 path)
    uint32_t pad_e8;           // +0xE8: unknown
    uint32_t account_id;       // +0xEC: *** CRITICAL - netmgr+136,+140 ***
    uint8_t  farm_slots[3][40];// +0xF0..+0x167: 3 farm slot records
    uint32_t num_char_slots;   // +0x16C
    uint8_t  pad_170[0x10];    // +0x170..+0x17F: pad to 0x180
};
// sizeof = 0x180 = 384 bytes
#pragma pack(pop)
```

---

## Bug in prior proud_rmi.py

Old code:
```python
struct.pack_into("<I", body, 8, account_id)   # WRONG offset
# +0xEC never written → game stored account_id=0
```

Fix:
```python
struct.pack_into("<I", body, 0x08, session_uid)   # actual semantic at +8
struct.pack_into("<I", body, 0xEC, account_id)    # correct account_id offset
```

Also: name buffer is wchar[38] = 76 bytes ending at +0x5B. Writing >37 chars would overwrite
field_5c at +0x5C. Old code truncated at 260 chars (520 bytes) which was catastrophic for
longer usernames (though "Player" = 6 chars was fine in practice).

---

## Open questions

- What exactly should `session_uid` be? Currently defaults to 1. sub_890AA0 uses it for session
  tracking; if the game doesn't find a matching session, it may silently fail.
- `field_5c`, `field_60`: farm slot count / character count? Setting to 0 might skip farm selection.
- `farm_slots[3][40]`: farm data records. All zeros should cause the game to show no farms.
- `field_e0`, `field_e4`: only used by sub_65EAA0 path (alternate handler). May be irrelevant if
  sub_4BA070 is the one actually called at runtime.
- Whether advancing past `connecting_to_server` requires also sending RMI_NET_CONNECT_RES (0x3E8E)
  and/or NOTIFY (0x3E99) before or after - not yet verified with a fresh run.
