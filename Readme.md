# TheGame (reimplemented)
This repo contains C++ code in an attempt to reimplement a few parts of "The Game" from scratch.
Everything is built into DLL that hooks the original methods on launch.

## Prerequisites
- Visual Studio 2022 (Build tools)
    - MSVC v143 - VS 2022 C++ x64/x86 Build tools
    - ATL C++ for v143 (make sure it is the same version as MSVC v143)
- CMake 3.15+
- Your brain

## Build instructions

### Command line
```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

### VS Code with CMake Tools extension
- CMake: Configure (Windows MSVC x64_x86 (Ninja) Release)
- CMake: Build

### Vscode clangd extension setup

add this to `.vscode/settings.json`:
```json
{
    "cmake.useCMakePresets": "always",
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}/build/msvc-x64_x86-debug"
    ]
}
```

### Launch

**One-time elevated daemon** (confirms UAC once; keeps automation unblocked):

```powershell
gsudo python launch/ctl/thegame_ctl.py -d
```

**Non-elevated** (agents, scripts) - or use [just](https://github.com/casey/just) from repo root:

```powershell
just daemon-bg          # once, via gsudo
just ping
just diagnostics-run
just kill
just copy-logs
```

Direct Python equivalents:

```powershell
python launch/ctl/thegame_ctl.py ping
python launch/ctl/thegame_ctl.py diagnostics-run
python launch/ctl/thegame_ctl.py kill --all
python launch/ctl/thegame_ctl.py copy-logs
```

Session artifacts live under `logs/runs/<run_id>/` (`events.jsonl`, `game_logs.txt`, `game_netlogs.txt`, `meta.json`).

Legacy wrapper (same as `diagnostics-run`): `python launch/diagnostics_controller.py`

Simple launch without diagnostics pipe: `launch/launch_game.py` (may still need the daemon for elevated spawn in some setups).