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
cmake --build build --config Debug
```

### Justfile

You must have [just command runner](https://github.com/casey/just) installed

```bash
just build
```

### VS Code with CMake Tools extension

- CMake: Configure (Windows MSVC x64_x86 (Ninja) Debug)
- CMake: Build

### Vscode clangd extension setup

Add this to `.vscode/settings.json`:

```json
{
    "cmake.useCMakePresets": "always",
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}/build/msvc-x64_x86-debug"
    ]
}
```

And compile the Debug target once.

### Launch

This section assumes you have unpacked version of The Game.

Install the [controller package](https://github.com/stndart/GameController) and start the elevated daemon once:

```bash
just ctl::daemon-bg
```

Then you can interact either via `just` commands or mcp tool the controller provides.

```bash
just ctl::ping              # check if controller is alive
just ctl::copy-dll          # copy DLL from build/msvc-x86-debug/bin/
just ctl::launch            # run the game
```

Controller lives under `[ctl/](ctl/)`; see `[ctl/README.md](ctl/README.md)` for all commands (`wait-stage`, `copy-logs`, etc.).

### Offline launch

You can override live server ip with setting env var `THEGAME_SERVER_OVERRIDE=ON` and specifying your desired ip to `THEGAME_SERVER_IP` (which is 127.0.0.1 by default).

## Logs

By default, this module spawns a console and prints most of the stuff there.
It also mirrors anything printed to `logs.txt` in game folder.

If a controller is attached, it sends messages to the `\\.\pipe\thegame-diagnostics` and accepts commands through `\\.\pipe\thegame-handler`. This even includes fatal exceptions such as AV.

Network events are separate: this module only mentions them in common logs and dumps full packets to `netlogs.txt`. You can disable writing network logs at all setting `NO_NETWORK_LOGS` as a compile flag or env var. You can also silent keepalive packets with `SILENT_KEEPALIVE` flag and mentioning network events in the main log with `SILENT_NETWORK` (this leaves `netlogs.txt` active).

For ProudNet handlers there is a separate log `proudlogs.txt`. The flags are `NO_PROUD_LOGS`, `SILENT_PROUD`.

## Compile targets

These are just basically some flags combinations

- release
- debug
- debug-headless: `THEGAME_NO_CONSOLE=ON`
- debug-no-hooks: `DISABLE_HOOKS=ON`
- debug-max: `DISABLE_PARK_THREAD=OFF`, `DISABLE_INT=OFF`, `MINIDUMP_ENABLED=ON`

## Flags

(both compile and environment)


| Flag                      | Default | What it does                                                                                           |
| ------------------------- | ------- | ------------------------------------------------------------------------------------------------------ |
| `THEGAME_NO_CONSOLE`      | OFF     | Disables console window                                                                                |
| `THEGAME_PIPES`           | ON      | Disabling turns off controller integration.                                                            |
| `DISABLE_HOOKS`           | OFF     | Disables hooks. Entirely. Why would you need that?                                                     |
| `DISABLE_SYSHOOKS`        | OFF     | Disables hooks for system calls. WS2, for example.                                                     |
| `DISABLE_ENTRYPOINT_HOOK` | OFF     | Disables entrypoint hook, which delays: pipes, VEH and some other stuff initialization.                |
| `DISABLE_VEH`             | OFF     | Disables custom VEH that captures exceptions.                                                          |
| `DISABLE_PARK_THREAD`     | ON      | Experimental - parks a thread when a fatal exception occurs.                                           |
| `DISABLE_INT`             | ON      | Experimental - raises a debug interrupt on exception. You would need to bypass AC to get this working. |
| `MINIDUMP_ENABLED`        | OFF     | Experimental - makes a minidump on fatal exception (AV).                                               |
| `DISABLE_RMI_INJECT`      | ON      | Experimental - the client *sometimes* acts as the upstream server, answering it's own RMIs.            |
| `DISABLE_AUTONAV`         | ON      | Experimental - disables autonavigation hooks                                                           |
| `NO_NETWORK_LOGS`         | OFF     |                                                                                                        |
| `SILENT_KEEPALIVE`        | ON      | Prevents logging pings on 20009 port.                                                                  |
| `SILENT_NETWORK`          | OFF     | Do not print network logs in main log.                                                                 |
| `NO_PROUD_LOGS`           | OFF     |                                                                                                        |
| `SILENT_PROUD`            | OFF     |                                                                                                        |


Compile flags only


| Flag                                                   | Default | What it does                                                  |
| ------------------------------------------------------ | ------- | ------------------------------------------------------------- |
| `WS2_HOOKS`                                            | ON      | Hooks WS2 send/sendto/wsasend system calls. And logs them.    |
| `DISABLE_GAME_STARTUP_TRASH`                           | ON      | Disables capturing StorageSystem error 7 and AVolute error 20 |
| `LOG_WCONVERT`                                         | OFF     | Logs when WString converts. You don't need it.                |
| `ALLOC_LOG`, `RESERVE_LOG`, `CONCAT_LOG`, `FORMAT_LOG` | OFF     | Some noise with strings. You don't need them.                 |


### Dev details

To print a log, call `logf` as you would call a printf. It prints the line to console (if present), log file, and emits message over diagnostics pipe. Also any OutputDebugString intercepted by VEH is also printed to the log. But not the diagnostics pipe.

To print an exception, call `exceptionf` with `EXCEPTION_POINTERS` pointer (from a VEH for example). That would do the same as `logf`, just has slightly another appearence and the controller would parse that as an exception as well.

The log does not make OutputDebugString calls. If you wish to have some, contact me.

Just as you can print an exception, you can print an event, or a `stage` as we call them. It's all the same, but with custom appearence and the controller has some perks with them.