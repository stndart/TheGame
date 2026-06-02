# Wire S2C RMI builders - long journal

See brief: `journals/2026-05-29-05-wire-s2c-rmi-builders.md`

## Wire format (three layers)

1. TCP: `[13 57][len_tag][len][payload]`
2. Transport opcode in payload (S2C game RMI uses `0x02`)
3. Game RMI: `[rmi_id: u16 LE][body]`

Lobby replay bursts are many layer-1 frames concatenated; inner opcodes are mostly `0x02` RMI (scan of `lobby_replay.json` burst 0 shows `0x3F4B`, `0x3E8E`, etc.).

## Handler layouts (IDA MCP, base 0x400000)

| RMI | RVA | Body gate |
|-----|-----|-----------|
| `0x3F30` | `0x437160` | `u16 result@+2`, 0 = success → `RequestState(9)` |
| `0x3ED4` | `0x4BB560` → `0x8DA900` | `i32 count@+2`, records 32 B stride |
| `0x3ED8` | `0x4BB370` | `i32 count@+2`, records 0xA8 stride |
| `0x3F81` | `0x4BB450` | `u32 state@+2` (not a result code) |
| `0x3F11` | `0x4BB7E0` | `u32` ids @+2, +6 |

No in-handler length checks - undersized bodies OOB.

## PCAP extraction (GitS_FA_Network_Captures)

Python/dpkt: 21k+ S2C frames. Priority ids on wire as bare `0x02`:

- `0x3F3E`: 10×, 640 B body (paste in `proud_rmi.py` as reference)
- `0x3F3D`: 38×, 42 B (version string)
- `0x3F30`, `0x3F2F`, `0x3F81`, `0x3ED4`: **not** in pcaps as plaintext S2C

## Server implementation

`build_create_room_res_burst()` = 242 B = 3 Proud frames (`0x3F30`, `0x3ED4`, `0x3ED8`).

Trigger when `GENERATE_CREATE_ROOM_RES_ON_REQ` and post-replay GAME `0x25` with `len(data)==115`.

In-DLL inject remains default for offline UX (`pn_rmi_inject.cpp`); disable with `THEGAME_DISABLE_RMI_INJECT=1` to test wire-only.

## Run 187 (`187_eafd5dd0`) - PASS

Wire log `wire_20260529T105008Z.log` lines 205-206:

- `RX :27380 ... op=0x25 (session) data=115B` (create-room C2S)
- `TX ... len=242B hex32=1357010b02303f...` (generated burst: `0x3F30` + `0x3ED4` + `0x3ED8`)

`events.jsonl`: `0x3F30`/`0x3AA0` len=98 → `game_stage: room` - **no** `inject: create-room` line.

UX: empty teams initially; player slot filled after Ready/Cancel churn; leave room (`0x3F45`) had no server RES.

## Open
- `0x3F3D` start-match wire body for map load.
- Whether recv-thread delivery of wire RES needs main-thread deferral (production server uses normal recv path).
