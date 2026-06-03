set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

default:
    @just --list

# --- build ---
# `just build` → debug; `just build <preset>` → msvc-x86-<preset> (see CMakePresets.json buildPresets).
# break-on-av: full hooks + VEH int3 on 0xC0000005 - debugger MUST be attached.

build target="debug":
    .\cmake-vs.bat --preset msvc-x86-{{target}}
    .\cmake-vs.bat --build --preset {{target}}
