# Long: offline server IP and stage hooks (2026-05-27)

## Launch path

```
just launch-offline → ctl launch --offline -s 127.0.0.1
  → write_config (config.ini m_serverIp)  # launcher only
  → write_server_override (GAME dir/TheGame.server_ip)  # TheGame.dll
  → GameLauncher.exe
  → GAME.exe + TheGame.dll
```

## IDA: hook trampolines

| Function | RVA | First 5 bytes | Was return_addr | Fixed |
|----------|-----|---------------|-----------------|-------|
| CGameIntro onPreProcess | 0x42A010 | 55 8B EC 83 E4 | 0x42A015 | 0x42A016 |
| CGameLogin onPreProcess | 0x42B280 | 55 8B EC 83 E4 | 0x42B285 | 0x42B286 |
| CGameServer Begin | 0x4345B0 | 55 8B EC 83 E4 | 0x4345B5 | 0x4345B6 |
| CGameServer End (main menu) | 0x4347CC | B9 88 72 6B 01 | 0x4347D1 | unchanged (mov ecx, not frame prologue) |

Broken trampoline executed `0xF8` as an instruction after replaying `and esp,-8` partially.

## options.dat

String xref `options.dat` @ 0x16E8290. IP not stored as plain ASCII in binary (IDA string search for `137.184.201.52` empty).

## Override semantics

- File: `<GAME.exe dir>/TheGame.server_ip` (one line, ASCII IP).
- Env override: `THEGAME_SERVER_IP` (wins over file if set).
- Remap only when connect host is `137.184.201.52` or `216.131.86.188` (lobby redirect per GITS proxy docs).
- Offline default when `--offline` and no `-s`: `127.0.0.1` via `resolve_server_ip(..., offline=True)`.

## Stage timeline (expected)

1. `started` - diagnostics pipe hello
2. `intro` - hook @ CGameIntro
3. `login` - hook @ CGameLogin
4. `connecting_to_server` - TCPSocket::Connect port 7000
5. `shard_choice` / `main_menu` - later hooks
