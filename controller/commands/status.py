from json import dumps
from typing import Literal

from config import Settings

from .common import Command
from .state import State


class StatusCommand(Command):
    command: Literal["status"] = "status"

    def invoke(self, settings: Settings, state: State) -> str:
        return dumps(
            {
                "run_id": state.run_id,
                "game_status": state.status,
                "game_pid": state.game_pid,
                "log_filepath": (
                    str(state.log_filepath) if state.log_filepath is not None else None
                ),
            }
        )
