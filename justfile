# FA-EMU launch control via controller/ (pip install -e . → `ctl` on PATH).
#
# One-time elevated daemon:
#   just daemon-bg
# Then (non-elevated):
#   just ping
#   just run-session

set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

ctl := "uv run ctl"
game_exe := 'G:\Games\FA\FA-EMU\Shipping\GAME.exe'

default:
    @just --list

# --- daemon (elevated; gsudo / UAC once) ---

daemon:
    gsudo {{ctl}} -d

daemon-bg:
    gsudo {{ctl}} -d --background

# --- client (non-elevated; requires daemon) ---

ping:
    {{ctl}} ping

processes:
    {{ctl}} processes

status:
    {{ctl}} status

stages:
    {{ctl}} stages

stop:
    {{ctl}} stop

kill:
    {{ctl}} kill

kill-all:
    {{ctl}} kill --all

copy-dll dll_config="debug":
    {{ctl}} copy-dll --dll-config {{dll_config}} -p "{{game_exe}}"

copy-logs:
    {{ctl}} copy-logs -p "{{game_exe}}"

copy-logs-run run_id:
    {{ctl}} copy-logs --run-id {{run_id}} -p "{{game_exe}}"

launch server_ip="":
    {{ctl}} launch -p "{{game_exe}}" -s "{{server_ip}}"

launch-offline server_ip="127.0.0.1":
    {{ctl}} launch -p "{{game_exe}}" -s "{{server_ip}}" --offline

wait-menu:
    {{ctl}} wait-for-stage main_menu --timeout 120

wait-stage stage timeout="120":
    {{ctl}} wait-for-stage {{stage}} --timeout {{timeout}}

run-session dll_config="debug":
    just copy-dll dll_config={{dll_config}}
    just launch
    just wait-menu
    just kill-all
    just copy-logs

# --- build (optional helper) ---

build-debug:
    .\cmake-vs.bat --build --preset debug

build-release:
    .\cmake-vs.bat --build --preset release
