# 2026-05-27 NetUserConnectRES struct + ProudNet frame layout (IDA-confirmed)

## Goal
IDA-verify the TCP :7000 connect path: ProudNet framing, RMI dispatch, and exact
NetUserConnectRES body layout so the Python server emits correct bytes.

## Functions analysed

| Address | Name | Role |
|---------|------|------|
| 0xD84BB0 | sub_D84BB0 | TCP frame reader (receiver loop) |
| 0xD84970 | sub_D84970 | TCP frame writer (sender) |
| 0xD85740 | sub_D85740 | Low-level 2-byte write → `sub_9FA180` |
| 0xD59250 | sub_D59250 | Length varint reader → sub_D9DFA0 |
| 0xD9DFA0 | sub_D9DFA0 | Varint decoder (switch on tag byte) |
| 0xD9DEC0 | sub_D9DEC0 | Varint encoder (identical thresholds) |
| 0xD9DE90 | sub_D9DE90 | 2-byte sub-reader for tag=2 |
| 0xD85800 | sub_D85800 | Insert element into 48-byte message queue |
| 0x4B8630 | sub_4B8630 | RMI dispatch table builder (registers 0x3F3E→sub_4BA070) |
| 0x4BA070 | sub_4BA070 | NetUserConnectRES handler ("onNetUserConnectRES") |

## Wire format (fully confirmed)

### Outer ProudNet TCP frame

```
[13 57] [tag] [len_bytes…] [payload…]
```

- `13 57`: magic word 0x5713 (LE). Confirmed at D84BB0+d84c68: `cmp word ptr ..., 5713h`.
  The byte 0x13 is **only** the low byte of the magic — not a separate protocol type.
- `tag`: 1-byte tag indicating length field width:
  - `01` → 1 byte length (signed char; max 127; D9DFA0 case 0)
  - `02` → 2 bytes LE (signed int16; max 32767; D9DFA0 case 1)
  - `04` → 4 bytes LE (int32; D9DFA0 case 3)
  - `08` → 8 bytes LE (int64; D9DFA0 case 7)
- Encoder (D9DEC0) uses identical thresholds:
  - `a+128 ≤ 255` → tag=1; `a+0x8000 ≤ 0xFFFF` → tag=2; `a+0x80000000 ≤ 0xFFFFFFFF` → tag=4
- **Python `encode_length` in `proud_frame.py` matches exactly.**

### Inner payload

```
[opcode: 1 byte] [rmi_id: 2 bytes LE] [body…]
```
opcode 0x02 = OP_MESSAGE (RMI). Confirmed: 0x4B86AD `mov dword ptr [esp+1Ch], 3F3Eh`
stores RMI ID 0x3F3E in the dispatch entry for sub_4BA070.

### "48-byte elements" (sub_D85800)

Sub_D85800 manages an **internal** message queue with 48-byte (0x30) elements —
stride `sub esi,30h`, field copies at -12, -4, +0, +4 from the "main pointer" at +0x28.
This is the game's network layer internal representation. **Not a wire format.** Server
need not know about it.

## NetUserConnectRES body (sub_4BA070 assembly)

Minimum safe size: **0x180 = 384 bytes** (zeroed; patch only the listed fields).

```
Offset  Size  Field           Assembly evidence
+0x002    2   error_code      cmp word ptr [esi+2], 0  (4ba09f)
+0x004    4   farm_select_id  mov ecx, [esi+4] → outgamemgr+36  (4ba185)
+0x008    4   session_uid     mov eax, [esi+8] → sub_890AA0  (4ba14a)
+0x00C    4   (unused)
+0x010   76   name wchar[38]  lea edi, [esi+10h] → wcscpy_s(buf, 260, edi)  (4ba15e)
                               max 37 chars + NUL; +0x5C begins next field
+0x05C    4   field_5c        mov eax, [esi+5Ch] → sub_416830  (4ba26f)
+0x060    4   field_60        mov edx, [esi+60h] → [charobj+0x107C]  (4ba269)
+0x064  140   _gap            zeroed; +0x70 passed to sub_415440  (4ba19e)
+0x0EC    4   account_id      mov eax, [esi+0ECh] → netmgr+0x88,+0x8C  (4ba0e8) ← CRITICAL
+0x0F0  120   CharSlot[3]     add esi,0F0h; loop 3×; stride 0x28  (4ba2a7–4ba300)
+0x168    4   (unused)
+0x16C    4   num_char_slots  mov eax, [edx+16Ch]  (4ba312)
+0x170   16   pad to 0x180    sub_65EAA0 reads up to +0x17C
```

## Bugs found in prior `proud_rmi.py`

| Bug | Fix |
|-----|-----|
| `account_id` was at +0x08 (was session_uid); real account_id at +0xEC was missing | Split into `session_uid`@+0x08 and `account_id`@+0xEC |
| `name.encode("utf-16-le")[:260*2]` — could overwrite `field_5c` at +0x5C | Cap name to 37 chars (`[:37*2]`) |
| `describe_frame` only handled tag=1,2; tag=4 used wrong header size | Added tag=4 → hdr=7 |

## Changes made

- `server/server/proud_rmi.py`: split session_uid/account_id, cap name, fix describe_frame
- `include/game/net/net_user_connect_res.hpp`: updated offsets table with IDA evidence
