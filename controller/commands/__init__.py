from typing import Literal

from config import Settings
from pydantic import TypeAdapter

from .common import Command
from .copy_dll import CopyDllCommand
from .copy_logs import CopyLogsCommand
from .kill import KillCommand
from .launch import LaunchCommand
from .ping import PingCommand
from .processes import ProcessesCommand
from .state import State
from .status import StatusCommand


class StopCommand(Command):
    command: Literal["stop"] = "stop"

    def invoke(self, settings: Settings, state: State) -> str:
        return "ok"


command_adapter = TypeAdapter(
    LaunchCommand
    | PingCommand
    | ProcessesCommand
    | StatusCommand
    | CopyDllCommand
    | CopyLogsCommand
    | KillCommand
    | StopCommand
)

__all__ = ["command_adapter", "State", "StopCommand"]
