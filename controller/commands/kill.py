"""Kill game processes (tracked launcher PID or by image name)."""

from __future__ import annotations

import subprocess
from json import dumps
from typing import Any, Literal

from config import Settings

from .common import Command
from .processes import DEFAULT_KILL_IMAGES
from .state import CommandError, State, Status


class KillCommandError(CommandError):
    pass


def _taskkill_by_image(image: str) -> dict[str, Any]:
    proc = subprocess.run(
        ["taskkill", "/F", "/IM", image],
        capture_output=True,
        text=True,
        check=False,
    )
    return {
        "image": image,
        "returncode": proc.returncode,
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


def _taskkill_by_pid(pid: int) -> dict[str, Any]:
    proc = subprocess.run(
        ["taskkill", "/F", "/PID", str(pid)],
        capture_output=True,
        text=True,
        check=False,
    )
    return {
        "pid": pid,
        "returncode": proc.returncode,
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


class KillCommand(Command):
    command: Literal["kill"] = "kill"
    all: bool = False

    def invoke(self, settings: Settings, state: State) -> str:
        if self.all:
            killed = [_taskkill_by_image(image) for image in DEFAULT_KILL_IMAGES]
            if state.status == Status.started:
                state.end_session()
            return dumps({"killed": killed})

        if state.game_pid is None:
            raise KillCommandError("No game running")

        pid = state.game_pid
        result = _taskkill_by_pid(pid)
        state.end_session()
        return dumps({"killed": [result]})
