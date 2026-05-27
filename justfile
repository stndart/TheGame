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

# --- controller ---

mod ctl

# --- server ---

serve:
    cd server | uv run serve

hot-serve:
    cd server | uv run hot-serve

# --- build (optional helper) ---

build-debug:
    .\cmake-vs.bat --build --preset debug

build-release:
    .\cmake-vs.bat --build --preset release
