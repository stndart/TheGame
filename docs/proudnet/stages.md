# ctl game stages (ProudNet-adjacent)

UI milestones emitted by `TheGame.dll` diagnostics. These are **observable outcomes**, not wire opcodes. Canonical string list: [`ctl/controller/gamestate/stages.py`](../../ctl/controller/gamestate/stages.py). Implementation: [`src/hooks/game_state.cpp`](../../src/hooks/game_state.cpp).

---

## Boot order

```text
started → intro → login → connecting_to_server → shard_select → lobby → …
```

`started` is session bookkeeping in ctl, not a hook. `connecting_to_server` is emitted from [`src/game/engine/TCPSocket.cpp`](../../src/game/engine/TCPSocket.cpp) when connecting to port **7000**, not from a UI `onPreProcess` hook.

---

## Hook table

| ctl stage | RVA | Hook symbol | Emits `stagef`? |
| --- | --- | --- | --- |
| `intro` | `0x42A010` | `g_target_game_intro` | yes |
| `login` | `0x42B280` | `g_target_game_login` | yes |
| *(none)* | `0x4345B0` | `g_target_game_server_select` | no — `server_begin` nav only |
| **`shard_select`** | `0x4347CC` | `g_target_game_main_menu` | yes |
| `lobby` | `0x42BD50` | `g_target_game_lobby` | yes |
| `room_list` | `0x4362E0` | `g_target_game_room_list` | yes |
| `party_room` | `0x42F690` | `g_target_game_party_room` | yes |
| `room` | `0x439B00` | `g_target_game_room` | yes |
| `char_select` | `0x4F2DB0` | `g_target_game_char_select` | yes |
| `map_loading` | `0x4806E0` | `g_target_game_map_loading` | yes |
| `in_game` | `0x47F610` | `g_target_game_in_game` | yes |

---

## Legacy names (do not use in new docs)

| Old name | Use instead |
| --- | --- |
| `shard_choice` | `shard_select` (ctl stage at `0x4347CC`) |
| `main_menu`, `server_ready` | `shard_select` |
| Stage at `0x4345B0` | No ctl stage — server/shard UI **begin** |

---

## ctl recipes

```powershell
just ctl::wait-stage shard_select 240
just ctl::wait-menu   # alias for shard_select
```

RMI-specific nav: [../rmi/autonav.md](../rmi/autonav.md).

*Last updated: 2026-06-03.*
