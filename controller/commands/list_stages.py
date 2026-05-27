"""List known game stages and stages seen in the current session."""

from __future__ import annotations

from json import dumps
from typing import Literal

from config import Settings
from stages import KNOWN_STAGES

from .common import Command
from .state import State


class ListStagesCommand(Command):
    command: Literal["list_stages"] = "list_stages"

    def invoke(self, settings: Settings, state: State) -> str:
        return dumps(
            {
                "known": list(KNOWN_STAGES),
                "seen": list(state.progress.game_states),
            }
        )
