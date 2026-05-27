"""Daemon command handlers."""

from __future__ import annotations

import ctypes
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable

import paths
from diagnostics_session import (
    DiagnosticsSessionConfig,
    copy_dll,
    copy_game_logs_to_run,
    run_diagnostics_session,
)
from protocol import encode_response

_LAUNCH_DIR = paths.LAUNCH_DIR
if str(_LAUNCH_DIR) not in sys.path:
    sys.path.insert(0, str(_LAUNCH_DIR))
import launch_game  # noqa: E402

DEFAULT_KILL_IMAGES = ["GAME.exe", "GameLauncher.exe"]

shell32 = ctypes.WinDLL("shell32", use_last_error=True)
shell32.IsUserAnAdmin.restype = ctypes.c_int


def is_elevated() -> bool:
    return bool(shell32.IsUserAnAdmin())


def cmd_ping(_args: dict[str, Any]) -> dict[str, Any]:
    return {"version": "1", "elevated": is_elevated()}


def cmd_status(_args: dict[str, Any]) -> dict[str, Any]:
    result: dict[str, list[dict[str, int]]] = {}
    for image in DEFAULT_KILL_IMAGES:
        proc = subprocess.run(
            ["tasklist", "/FI", f"IMAGENAME eq {image}", "/FO", "CSV", "/NH"],
            capture_output=True,
            text=True,
            check=False,
        )
        pids: list[int] = []
        for line in proc.stdout.splitlines():
            line = line.strip()
            if not line or "No tasks" in line:
                continue
            parts = [p.strip('"') for p in line.split('","')]
            if parts and parts[0].lower() == image.lower():
                try:
                    pids.append(int(parts[1]))
                except (ValueError, IndexError):
                    pass
        result[image] = pids
    return {"processes": result}


def cmd_kill(args: dict[str, Any]) -> dict[str, Any]:
    images = args.get("images") or DEFAULT_KILL_IMAGES
    killed: list[dict[str, Any]] = []
    for image in images:
        proc = subprocess.run(
            ["taskkill", "/F", "/IM", image],
            capture_output=True,
            text=True,
            check=False,
        )
        killed.append(
            {
                "image": image,
                "returncode": proc.returncode,
                "stdout": proc.stdout.strip(),
                "stderr": proc.stderr.strip(),
            }
        )
    return {"killed": killed}


def cmd_copy_dll(args: dict[str, Any]) -> dict[str, Any]:
    game_exe = Path(args.get("game_exe", str(launch_game.GAME_PATH))).resolve()
    dll_config = args.get("dll_config", "debug")
    dll_source = args.get("dll_source")
    info = copy_dll(game_exe, dll_config, dll_source)
    return info


def cmd_copy_logs(args: dict[str, Any]) -> dict[str, Any]:
    run_id = paths.resolve_run_id(args.get("run_id"))
    game_exe = Path(args.get("game_exe", str(launch_game.GAME_PATH))).resolve()
    copied = copy_game_logs_to_run(game_exe, run_id)
    return {"run_id": run_id, "copied": copied}


def cmd_diagnostics_run(
    args: dict[str, Any],
    *,
    write_stream: Callable[[str], None],
    req_id: int,
) -> dict[str, Any]:
    game_exe = Path(args.get("game_exe", str(launch_game.GAME_PATH))).resolve()
    run_id = args.get("run_id") or paths.new_run_id()

    def on_progress(msg: str) -> None:
        write_stream(encode_response(req_id, ok=True, msg_type="progress", message=msg))

    def on_event(line: str) -> None:
        write_stream(encode_response(req_id, ok=True, msg_type="event", line=line))

    config = DiagnosticsSessionConfig(
        game_exe=game_exe,
        launch_token=str(args["launch_token"]),
        server_ip=str(args["server_ip"]),
        kernel_check_disable=bool(args.get("kernel_check_disable")),
        run_id=run_id,
        dll_config=args.get("dll_config", "debug"),
        dll_source=args.get("dll_source"),
        no_copy_dll=bool(args.get("no_copy_dll")),
        collect_game_logs=not bool(args.get("no_collect_game_logs")),
    )
    return run_diagnostics_session(config, on_progress=on_progress, on_event=on_event)


HANDLERS: dict[str, Callable[..., dict[str, Any]]] = {
    "ping": cmd_ping,
    "status": cmd_status,
    "kill": cmd_kill,
    "copy_dll": cmd_copy_dll,
    "copy_logs": cmd_copy_logs,
}
