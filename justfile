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

mod server

# --- build (optional helper) ---

build-debug:
    .\cmake-vs.bat --preset msvc-x86-debug
    .\cmake-vs.bat --build --preset debug

# Debug DLL with in-process RMI injection compiled out (wire-only server tests).
build-debug-wire:
    .\cmake-vs.bat --preset msvc-x86-debug-wire
    .\cmake-vs.bat --build --preset debug-wire

copy-dll-wire: build-debug-wire
    just ctl::copy-dll debug-wire

# Offline launch with THEGAME_NAV_AUTO=create_room (must use this for autonav).
launch-offline-nav:
    just ctl::launch-offline-nav

# Offline launch with THEGAME_NAV_AUTO=exit_lobby (lobby back to shard picker).
launch-offline-exit-nav:
    just ctl::launch-offline-exit-nav

run-exit-lobby-test:
    just ctl::run-exit-lobby-test

build-release:
    .\cmake-vs.bat --build --preset release
