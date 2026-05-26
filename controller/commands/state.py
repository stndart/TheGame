from enum import StrEnum, auto
from pathlib import Path
from threading import Thread
from time import sleep
from typing import TextIO

from pipe import PipeServer


class Status(StrEnum):
    created = auto()
    started = auto()
    stopped = auto()


class State:
    _running: bool = False
    status: Status
    game_pid: int | None = None
    diag_thread: Thread
    diag_pipe: PipeServer
    log_filepath: Path | None = None
    log_file: TextIO | None = None

    def __init__(self, diag_pipe: PipeServer):
        self.status = Status.created
        self.diag_pipe = diag_pipe
        self.diag_thread = Thread(target=self.diag_thread_func)

    def start(self, game_pid: int):
        self._running = True
        self.status = Status.started
        self.game_pid = game_pid

        self.diag_thread.start()
        self.log_filepath = Path(f"logs/game_{self.game_pid}.log")
        self.log_file = self.log_filepath.open("w+", encoding="utf-8")
        self.log_file.write(f"Game started: pid={self.game_pid}\n")
        self.log_file.flush()

    def diag_thread_func(self):
        assert self.log_file is not None

        while self._running and not self.diag_pipe.accept():
            sleep(0.1)

        while self._running:
            try:
                line = self.diag_pipe.read()
                self.log_file.write(line)
                self.log_file.flush()
            except Exception as e:
                self.log_file.write(f"Error: {e}\n")
                self.log_file.flush()
                break
        self.diag_pipe.disconnect()
        self.stop()

    def stop(self):
        self._running = False
        self.status = Status.stopped
        self.game_pid = None
        self.diag_thread.join()

        if self.log_file is not None:
            self.log_file.close()


class CommandError(Exception):
    pass
