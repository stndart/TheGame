"""Elevated diagnostics pipe session (game launch + event collection)."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import paths
from pipe_io import (
    DIAGNOSTICS_PIPE_NAME,
    create_diagnostics_server_pipe,
    kernel32,
    read_line,
    wait_for_diagnostics_client,
)

# launch_game lives in parent launch/ directory
_LAUNCH_DIR = paths.LAUNCH_DIR
if str(_LAUNCH_DIR) not in sys.path:
    sys.path.insert(0, str(_LAUNCH_DIR))
import launch_game  # noqa: E402


ProgressFn = Callable[[str], None]
EventFn = Callable[[str], None]


@dataclass
class DiagnosticsSessionConfig:
    game_exe: Path
    launch_token: str
    server_ip: str
    kernel_check_disable: bool = False
    run_id: str = ""
    dll_config: str = "debug"
    dll_source: str | None = None
    no_copy_dll: bool = False
    collect_game_logs: bool = True
    extra_meta: dict[str, Any] = field(default_factory=dict)


def resolve_dll_source(dll_config: str, dll_source: str | None) -> Path:
    if dll_source:
        return Path(dll_source).resolve()
    path = paths.DLL_CONFIGS.get(dll_config)
    if not path:
        raise ValueError(f"unknown dll_config: {dll_config}")
    return path.resolve()


def copy_dll(game_exe: Path, dll_config: str, dll_source: str | None) -> dict[str, str]:
    source = resolve_dll_source(dll_config, dll_source)
    if not source.is_file():
        raise FileNotFoundError(
            f"TheGame.dll not found for {dll_config}: {source}. Build first or pass dll_source."
        )
    target = game_exe.with_name(source.name)
    shutil.copy2(source, target)
    return {"source": str(source), "target": str(target)}


def copy_game_logs_to_run(game_exe: Path, run_id: str) -> dict[str, str]:
    shipping = game_exe.parent
    dest_dir = paths.run_dir(run_id)
    copied: dict[str, str] = {}
    pairs = [
        (paths.SHIPPING_LOGS_FILE, paths.GAME_LOGS_FILE),
        (paths.SHIPPING_NETLOGS_FILE, paths.GAME_NETLOGS_FILE),
    ]
    for src_name, dest_name in pairs:
        src = shipping / src_name
        if src.is_file():
            dest = dest_dir / dest_name
            shutil.copy2(src, dest)
            copied[dest_name] = str(dest)
    return copied


def read_events_to_file(
    handle,
    output_path: Path,
    *,
    on_event: EventFn | None = None,
) -> int:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    pending = b""
    count = 0

    with output_path.open("w", encoding="utf-8") as out:
        while True:
            line, pending = read_line(handle, pending)
            if line is None:
                break
            count += 1
            out.write(line + "\n")
            out.flush()
            if on_event:
                on_event(line)

    return count


def write_meta(run_dir_path: Path, meta: dict[str, Any]) -> None:
    meta_path = run_dir_path / paths.META_FILE
    meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")


def run_diagnostics_session(
    config: DiagnosticsSessionConfig,
    *,
    on_progress: ProgressFn | None = None,
    on_event: EventFn | None = None,
) -> dict[str, Any]:
    def progress(msg: str) -> None:
        if on_progress:
            on_progress(msg)

    game_exe = config.game_exe.resolve()
    if not game_exe.is_file():
        raise FileNotFoundError(f"GAME.exe not found: {game_exe}")
    if not launch_game.GAME_LAUNCHER_EXE.is_file():
        raise FileNotFoundError(
            f"GameLauncher.exe not found: {launch_game.GAME_LAUNCHER_EXE}"
        )

    run_id = config.run_id or paths.new_run_id()
    run_dir_path = paths.run_dir(run_id)
    events_file = paths.events_path(run_id)
    started_at = datetime.now(timezone.utc).isoformat()

    if not config.no_copy_dll:
        dll_info = copy_dll(game_exe, config.dll_config, config.dll_source)
        progress(f"dll copied: {dll_info['target']}")
    else:
        dll_info = {}

    diag_pipe = create_diagnostics_server_pipe()
    try:
        proc = launch_game.spawn_game_launcher(
            config.launch_token, config.kernel_check_disable
        )
        progress(
            f"launcher_pid={proc.pid} server={config.server_ip} "
            f"waiting for {DIAGNOSTICS_PIPE_NAME}"
        )

        wait_for_diagnostics_client(diag_pipe)
        progress("diagnostics connected")

        event_count = read_events_to_file(
            diag_pipe, events_file, on_event=on_event
        )
        progress(f"diagnostics disconnected after {event_count} events")

        ended_at = datetime.now(timezone.utc).isoformat()
        game_logs: dict[str, str] = {}
        if config.collect_game_logs:
            game_logs = copy_game_logs_to_run(game_exe, run_id)
            if game_logs:
                progress(f"collected game logs: {', '.join(game_logs.keys())}")

        meta = {
            "run_id": run_id,
            "run_dir": str(run_dir_path.resolve()),
            "started_at": started_at,
            "ended_at": ended_at,
            "game_exe": str(game_exe),
            "server_ip": config.server_ip,
            "dll_config": config.dll_config,
            "dll": dll_info,
            "launcher_pid": proc.pid,
            "events": event_count,
            "events_file": str(events_file.resolve()),
            "game_logs": game_logs,
            **config.extra_meta,
        }
        write_meta(run_dir_path, meta)
        paths.write_last_run(run_id, run_dir_path)

        return {
            "run_id": run_id,
            "run_dir": str(run_dir_path.resolve()),
            "events": event_count,
            "events_file": str(events_file.resolve()),
            "meta_file": str((run_dir_path / paths.META_FILE).resolve()),
            "game_logs": game_logs,
        }
    finally:
        kernel32.CloseHandle(diag_pipe)
