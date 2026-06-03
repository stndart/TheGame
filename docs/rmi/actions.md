# Verified action → RMI map

Condensed C2S/S2C pairs from client hooks and friends-server integration. S2C must come from the **friends dummy server** on the wire — no in-process inject.

| Action | REQ proxy | REQ floor | len | RES id → leaf |
| --- | --- | --- | --- | --- |
| lobby enter | `0x3F40` | `0x3ACE` | 2 | `0x3F41` → `sub_4BA740` |
| global chat | - | `0x3AD2` | ~36 | echo RES |
| Quick Dive | `0x3EE4` | `0x3AAF` | 3 | ? |
| room list | `0x3F2F` | `0x3A9F` | 2 | `0x3F2F` → `sub_437390` |
| **create room** | **`0x3F30`** | **`0x3AA0`** | **98** | **`0x3F30`** → **`sub_437160`** |
| room enter | `0x3ED3` | `0x3AC0` | 6 | `0x3ED4` → `sub_4BB560` |
| ready | `0x3F2B` | `0x3AA8` | 3 | **unknown** — see note below |
| team/loadout | - | `0x3AC6` | 10246 | ? |
| **start match** | **`0x3F3D`** | **`0x3AA9`** | 2 | **`0x3F3D`** → **`sub_43D9B0`** |
| **leave room** | **`0x3F45`** | **`0x3AA3`** | 6 | **`0x3F45`** → **`sub_43D020`** |

### Ready (`0x3F2B`) — RES id open

The old in-process inject harness (see [fake-server-hooks.md](fake-server-hooks.md)) latched Ready and pumped **`0x3F3D`** (start-match RES). That path is **removed** and was never verified against friends-server wire. Do **not** treat `0x3F3D` as the Ready answer.

Confirm the real S2C id with `[rmi] s2c` logs after a Ready click against the friends server, then update this table.

### Create-room REQ body (98 B)

| off | type | meaning |
| --- | --- | --- |
| +0 | u16 | floor id `15008` |
| +2 | WCHAR[~26] | room name |
| ~+0x36 | u8 | password flag |
| ~+0x38 | WCHAR[] | password |
| ~+0x42 | u32×6 | mode, channel, max players, map, … |

Create RES: `result@+2 == 0` sufficient.

### Start RES body (`sub_43D9B0`)

| off | type | use |
| --- | --- | --- |
| +2 | u16 | result |
| +4 | WCHAR[] | room/map name |
| +0x26 | u16 | game mode |
| +0x28 | u32 | match handle → `flt_1C25E2C+0x210` |

More detail: [client.md](client.md). IDA tables: [../rmi-ida-reimpl.md](../rmi-ida-reimpl.md).

*Last updated: 2026-06-03.*
