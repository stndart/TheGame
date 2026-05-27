"""Block until a game_state stage is observed (or timeout)."""

from __future__ import annotations

import sys
from json import dumps
from typing import Literal

from config import Settings
from stages import is_known_stage

from .common import Command
from .state import CommandError, State


class WaitForStageCommand(Command):
    command: Literal["wait_for_stage"] = "wait_for_stage"
    stage: str
    timeout: float = 120.0

    def invoke(self, settings: Settings, state: State) -> str:
        if state.status.value != "started" and state.game_pid is None:
            raise CommandError("No active session; launch the game first")

        if not is_known_stage(self.stage):
            print(
                f"warning: stage {self.stage!r} is not in KNOWN_STAGES",
                file=sys.stderr,
            )

        reached = state.wait_for_stage(self.stage, self.timeout)
        return dumps(
            {
                "reached": reached,
                "stage": self.stage,
                "current_stage": state.current_stage,
                "game_states": list(state.progress.game_states),
            }
        )
