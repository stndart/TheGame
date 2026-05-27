"""Copy shipping game logs into the run directory."""

from __future__ import annotations

import shutil
from json import dumps
from pathlib import Path
from typing import Literal

from config import Settings
from launch_game import Settings as LaunchSettings
from paths import (
    GAME_LOGS_FILE,
    GAME_NETLOGS_FILE,
    SHIPPING_LOGS_FILE,
    SHIPPING_NETLOGS_FILE,
    merge_meta,
    resolve_run_id,
    run_dir,
)

from .common import Command
from .state import CommandError, State


class CopyLogsCommandError(CommandError):
    pass


def clear_shipping_game_logs(game_exe: Path) -> None:
    shipping = game_exe.parent
    for name in (SHIPPING_LOGS_FILE, SHIPPING_NETLOGS_FILE):
        path = shipping / name
        if path.is_file():
            path.unlink()


def copy_game_logs_to_run(game_exe: Path, run_id: str) -> dict[str, str]:
    shipping = game_exe.parent
    dest_dir = run_dir(run_id)
    copied: dict[str, str] = {}
    pairs = [
        (SHIPPING_LOGS_FILE, GAME_LOGS_FILE),
        (SHIPPING_NETLOGS_FILE, GAME_NETLOGS_FILE),
    ]
    for src_name, dest_name in pairs:
        src = shipping / src_name
        if src.is_file():
            dest = dest_dir / dest_name
            shutil.copy2(src, dest)
            copied[dest_name] = str(dest)
    return copied


class CopyLogsCommand(Command):
    command: Literal["copy_logs"] = "copy_logs"
    run_id: str | None = None
    game_exe: str | None = None

    def invoke(self, settings: Settings, state: State) -> str:
        try:
            run_id = resolve_run_id(self.run_id or state.run_id)
        except ValueError as e:
            raise CopyLogsCommandError(str(e)) from e

        launch_settings = LaunchSettings()
        if self.game_exe is not None:
            launch_settings.GAME_PATH = Path(self.game_exe)

        game_exe = launch_settings.GAME_PATH.resolve()
        copied = copy_game_logs_to_run(game_exe, run_id)
        if copied:
            merge_meta(run_id, {"game_logs": copied})

        return dumps({"run_id": run_id, "copied": copied})
