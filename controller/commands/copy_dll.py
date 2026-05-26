import shutil
from json import dumps
from pathlib import Path
from typing import Literal

from config import Settings
from launch_game import Settings as LaunchSettings

from .common import Command
from .state import CommandError, State

DllConfig = Literal["debug", "release"]


class CopyDllCommandError(CommandError):
    pass


def resolve_dll_source(
    settings: Settings, dll_config: DllConfig, dll_source: str | None
) -> Path:
    if dll_source:
        return Path(dll_source).resolve()
    path = settings.dll_configs.get(dll_config)
    if path is None:
        raise CopyDllCommandError(f"unknown dll_config: {dll_config}")
    return path.resolve()


class CopyDllCommand(Command):
    command: Literal["copy_dll"] = "copy_dll"
    dll_config: DllConfig = "debug"
    dll_source: str | None = None
    game_exe: str | None = None

    def invoke(self, settings: Settings, state: State) -> str:
        launch_settings = LaunchSettings()
        if self.game_exe is not None:
            launch_settings.GAME_PATH = Path(self.game_exe)

        game_exe = launch_settings.GAME_PATH.resolve()
        if not game_exe.is_file():
            raise CopyDllCommandError(f"GAME.exe not found: {game_exe}")

        source = resolve_dll_source(settings, self.dll_config, self.dll_source)
        if not source.is_file():
            raise CopyDllCommandError(
                f"TheGame.dll not found for {self.dll_config}: {source}. "
                "Build first or pass dll_source."
            )

        target = game_exe.with_name(source.name)
        shutil.copy2(source, target)
        return dumps({"source": str(source), "target": str(target)})
