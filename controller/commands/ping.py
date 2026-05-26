import ctypes
from json import dumps
from typing import Literal

from config import Settings

from .common import Command
from .state import State

shell32 = ctypes.WinDLL("shell32", use_last_error=True)


class PingCommand(Command):
    command: Literal["ping"] = "ping"

    def invoke(self, settings: Settings, state: State) -> str:
        return dumps({"version": "1", "elevated": bool(shell32.IsUserAnAdmin())})
