# FA-EMU launch control - wraps launch/ctl/thegame_ctl.py (PowerShell on Windows).
#
# One-time elevated daemon:
#   just daemon-bg
# Then (non-elevated):
#   just ping
#   just diagnostics-run
#   just kill

set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

python := "python"
ctl := "launch/ctl/thegame_ctl.py"
game_exe := 'G:\Games\FA\FA-EMU\Shipping\GAME.exe'

default:
    @just --list

# --- daemon (elevated; gsudo / UAC once) ---

# Foreground daemon (blocks terminal)
daemon:
    gsudo {{python}} {{ctl}} -d

# Detached daemon (recommended)
daemon-bg:
    gsudo {{python}} {{ctl}} -d --background

# --- client (non-elevated; requires daemon) ---

ping:
    {{python}} {{ctl}} ping

status:
    {{python}} {{ctl}} status

kill:
    {{python}} {{ctl}} kill --all

# Stops the daemon
stop:
    {{python}} {{ctl}} stop

copy-dll dll_config="debug":
    {{python}} {{ctl}} copy-dll --dll-config {{dll_config}} -p "{{game_exe}}"

copy-dll-release:
    just copy-dll dll_config=release

# Uses logs/ctl/last_run.json when run_id omitted
copy-logs:
    {{python}} {{ctl}} copy-logs -p "{{game_exe}}"

copy-logs-run run_id:
    {{python}} {{ctl}} copy-logs --run-id {{run_id}} -p "{{game_exe}}"

diagnostics-run dll_config="debug":
    {{python}} {{ctl}} diagnostics-run --dll-config {{dll_config}} -p "{{game_exe}}"

diagnostics-run-release:
    just diagnostics-run dll_config=release

# Pass extra flags through, e.g.: just diagnostics-run-args --run-id 20260525T113124Z
diagnostics-run-args *ARGS:
    {{python}} {{ctl}} diagnostics-run -p "{{game_exe}}" {{ARGS}}

# Legacy wrapper (same RPC as diagnostics-run)
diagnostics-controller *ARGS:
    {{python}} launch/diagnostics_controller.py {{ARGS}}

# --- build (optional helper) ---

build-debug:
    .\cmake-vs.bat --build --preset debug

build-release:
    .\cmake-vs.bat --build --preset release
