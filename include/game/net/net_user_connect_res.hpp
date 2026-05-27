#pragma once

#include <cstddef>
#include <cstdint>

// Application payload for RMI NetUserConnectRES (msg id 0x3F3E / 16190).
// Handler: sub_4BA070 @ 0x4BA070 ("onNetUserConnectRES").
// Payload pointer arrives at handler as MsgDelegateArg+8.
//
// IDA-confirmed field-by-field from sub_4BA070 assembly:
//
//  Offset  Size  Access instruction            Notes
//  +0x000    2   (unused)
//  +0x002    2   cmp word ptr [esi+2], 0       error_code; 0 = success path
//  +0x004    4   mov ecx, [esi+4]              farm_select_id → outgamemgr+0x24
//  +0x008    4   mov eax, [esi+8]              session_uid → sub_890AA0 (NOT account_id)
//  +0x00C    4   (unused)
//  +0x010   76   lea edi, [esi+10h] → wcscpy_s name wchar_t[38]; max 37 visible chars + NUL
//  +0x05C    4   mov eax, [esi+5Ch]            → sub_416830 (farm-slot selector)
//  +0x060    4   mov edx, [esi+60h]            → player.slot_table[0x107C]
//  +0x064  140   (zeroes safe; +0x70 passed to sub_415440)
//  +0x0EC    4   mov eax, [esi+0ECh]           account_id → netmgr+0x88 and +0x8C (CRITICAL)
//  +0x0F0  120   CharSlot[3]; each 40 bytes    loop: add esi,0F0h; stride 0x28; 3 iters
//  +0x168    4   (unused)
//  +0x16C    4   mov eax, [edx+16Ch]           num_char_slots
//  +0x170   16   sub_65EAA0 reads up to +0x17C; pad to 0x180 for safety
//  Total = 0x180 = 384 bytes (server emits exactly this many zeroed + patched bytes)
namespace NetUserConnectResOff {
inline constexpr size_t kErr            = 0x002;  // uint16
inline constexpr size_t kFarmSelectId   = 0x004;  // uint32
inline constexpr size_t kSessionUid     = 0x008;  // uint32 (→ sub_890AA0)
inline constexpr size_t kName           = 0x010;  // wchar_t[38], max 37 chars + NUL
inline constexpr size_t kField5C        = 0x05C;  // uint32
inline constexpr size_t kField60        = 0x060;  // uint32
inline constexpr size_t kAccountId      = 0x0EC;  // uint32 (→ netmgr+136,140)
inline constexpr size_t kCharSlots      = 0x0F0;  // CharSlot[3], 40B each
inline constexpr size_t kNumCharSlots   = 0x16C;  // uint32
inline constexpr size_t kMinBodySize    = 0x170;
inline constexpr size_t kSafeBodySize   = 0x180;  // emitted by server
inline constexpr size_t kNameMaxChars   = 37;     // wchars excl. NUL; +0x5C would be overwritten otherwise
} // namespace NetUserConnectResOff
