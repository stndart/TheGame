"""Report current daemon session state and progress."""

from __future__ import annotations

from json import dumps
from typing import Literal

from config import Settings
from paths import events_path

from .common import Command
from .state import State


class StatusCommand(Command):
    command: Literal["status"] = "status"

    def invoke(self, settings: Settings, state: State) -> str:
        events_file = None
        run_dir = None
        if state.run_id:
            events_file = str(events_path(state.run_id).resolve())
            run_dir = str(state.run_dir_path.resolve()) if state.run_dir_path else None

        return dumps(
            {
                "run_id": state.run_id,
                "game_status": state.status.value,
                "game_pid": state.game_pid,
                "current_stage": state.current_stage,
                "game_states": list(state.progress.game_states),
                "progress": state.progress.to_dict(),
                "events_count": state.progress.events_count,
                "run_dir": run_dir,
                "events_file": events_file,
            }
        )
