# TheGame (reimplemented)
This repo contains C++ code in an attempt to reimplement a few parts of "The Game" from scratch.
Everything is built into DLL that hooks the original methods on launch.

## Prerequisites
- Visual Studio 2022 (Build tools)
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