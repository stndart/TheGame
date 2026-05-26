import subprocess
from json import dumps
from typing import Literal

from config import Settings

from .common import Command
from .state import State

DEFAULT_KILL_IMAGES = ["GAME.exe", "GameLauncher.exe"]


class ProcessesCommand(Command):
    command: Literal["processes"] = "processes"

    def invoke(self, settings: Settings, state: State) -> str:
        result: dict[str, list[int]] = {}
        for image in DEFAULT_KILL_IMAGES:
            proc = subprocess.run(
                ["tasklist", "/FI", f"IMAGENAME eq {image}", "/FO", "CSV", "/NH"],
                capture_output=True,
                text=True,
                check=False,
            )
            pids: list[int] = []
            for line in proc.stdout.splitlines():
                line = line.strip()
                if not line or "No tasks" in line:
                    continue
                parts = [p.strip('"') for p in line.split('","')]
                if parts and parts[0].lower() == image.lower():
                    try:
                        pids.append(int(parts[1]))
                    except (ValueError, IndexError):
                        pass
            result[image] = pids
        return dumps({"processes": result})
