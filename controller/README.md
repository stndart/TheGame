# thegame-ctl (`controller/`)

Elevated Windows daemon plus a non-elevated CLI for launching FA-EMU with TheGame.dll diagnostics. Commands are **composable** (copy DLL → launch → wait for stage → kill → copy logs); there is no monolithic `diagnostics-run` RPC.

## Setup

```powershell
cd /path/to/TheGame
pip install -e .
```

Config files (optional):


| File      | Purpose                                                 |
| --------- | ------------------------------------------------------- |
| `ctl.env` | Pipe names, DLL build paths (`Settings` in `config.py`) |
| `.env`    | Game path, API, account token (`launch_game.Settings`)  |


**One-time elevated daemon** (UAC / gsudo):

```powershell
gsudo ctl -d
# or detached:
gsudo ctl -d --background
# or:
just daemon-bg
```

Daemon prints `[stage] …` lines when the game reports new `game_state` phases.

## Typical workflow

```powershell
just copy-dll
just launch
just wait-menu          # ctl wait-for-stage main_menu --timeout 120
just kill
just copy-logs
```

Or the example chain:

```powershell
just run-session
```

Artifacts land in `logs/runs/<run_id>/`:


| File                                 | Content                                |
| ------------------------------------ | -------------------------------------- |
| `events.jsonl`                       | NDJSON diagnostics events from the DLL |
| `meta.json`                          | Run metadata, `game_states`, progress  |
| `game_logs.txt` / `game_netlogs.txt` | Copied from shipping dir (`copy-logs`) |


`logs/ctl/last_run.json` points at the latest run for `copy-logs` without `--run-id`.

## Stages

Known phases (see `stages.py`, keep in sync with `src/hooks/game_state.cpp`):


| Stage                  | When                       |
| ---------------------- | -------------------------- |
| `started`              | Diagnostics pipe connected |
| `intro`                | Game boot                  |
| `connecting_to_server` | Network connect            |
| `shard_choice`         | Shard selection UI         |
| `main_menu`            | Main menu                  |


```powershell
ctl stages                    # catalog + stages seen this session
ctl wait-for-stage main_menu  # blocks this RPC until stage or timeout
```

`wait-for-stage` only blocks the **client connection** handling that request; other commands can be issued from another terminal if needed (the daemon serves one RPC at a time per connection).

Exit code `1` if the stage was not reached before `--timeout` (default 120s).

## Commands


| CLI                       | Description                                                |
| ------------------------- | ---------------------------------------------------------- |
| `ctl -d`                  | Run daemon (foreground)                                    |
| `ctl -d --background`     | Detached daemon                                            |
| `ctl ping`                | Version and elevation check                                |
| `ctl processes`           | PIDs for GAME.exe / GameLauncher.exe                       |
| `ctl status`              | Session `run_id`, progress, stages, paths                  |
| `ctl stages`              | Known + seen stages                                        |
| `ctl launch`              | Fetch token (or `--offline`), write config, spawn launcher |
| `ctl wait-for-stage NAME` | Wait for diagnostics stage                                 |
| `ctl kill`                | Kill tracked launcher PID                                  |
| `ctl kill --all`          | Kill game images and end session                           |
| `ctl copy-dll`            | Copy TheGame.dll beside GAME.exe                           |
| `ctl copy-logs`           | Copy shipping logs into run dir                            |
| `ctl stop`                | Shut down daemon                                           |


Credentials are read from `store.json` / `.env` (`launch_game`); the CLI does not accept username/password flags.

## Troubleshooting

- **daemon not running** — start with `gsudo ctl -d`; client connect times out after 30s by default.
- **TheGame.dll not found** — `just build-debug` then `just copy-dll`.
- **wait-for-stage times out** — watch daemon `[stage]` logs; increase `--timeout` or confirm the hooked build is loaded.
- **No last run for copy-logs** — run `launch` first or pass `--run-id`.

## Legacy

`[launch/ctl/](../launch/ctl/)` remains for reference; new work should use this package and root `justfile` recipes.