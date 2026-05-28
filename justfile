# FA-EMU launch control via ctl/
#
# One-time elevated daemon:
#   just ctl::daemon-bg
# Then (non-elevated, from repo root - use ctl:: prefix):
#   just ctl::ping
#   just ctl::run-session-offline

set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

ctl := "uv run ctl"
game_exe := 'G:\Games\FA\FA-EMU\Shipping\GAME.exe'

default:
    @just --list

# --- controller ---

mod ctl

# --- server (singleton; prefer ensure-serve for agents) ---

serve:
    cd server | uv run serve

# Idempotent: start detached dummy server on :7000 if not already listening.
ensure-serve:
    cd server | uv run ensure-serve

serve-status:
    cd server | uv run serve-status

# Stop server process recorded in ctl/logs/ctl/server.lock (if still alive).
serve-stop:
    cd server | uv run serve-stop

hot-serve:
    @echo "hot-serve reloads on file changes; use ensure-serve for a stable background listener"
    cd server | uv run hot-serve

# --- build (optional helper) ---

build-debug:
    .\cmake-vs.bat --build --preset debug

build-release:
    .\cmake-vs.bat --build --preset release
