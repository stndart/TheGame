# FA-EMU launch control via ctl/
#
# One-time elevated daemon:
#   just ctl::daemon-bg
# Then (non-elevated, from repo root - use ctl:: prefix):
#   just ctl::ping
#   just ctl::launch

set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

default:
    @just --list

# --- controller ---

mod ctl

# --- server (singleton; prefer ensure-serve for agents) ---

mod server

# --- build (optional helper) ---

build:
    just build-debug

build-debug:
    .\cmake-vs.bat --preset msvc-x86-debug
    .\cmake-vs.bat --build --preset debug

build-release:
    .\cmake-vs.bat --preset msvc-x86-release
    .\cmake-vs.bat --build --preset release
