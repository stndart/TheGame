"""Daemon session state: diagnostics collection, stages, and run metadata."""

from __future__ import annotations

import json
import secrets
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import StrEnum, auto
from pathlib import Path
from threading import Condition, Thread
from time import monotonic, sleep
from typing import Any, TextIO

from paths import events_path, meta_path, run_dir, write_last_run
from pipe import PipeServer


def new_run_id() -> str:
    return secrets.token_hex(4)


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


class SessionPhase(StrEnum):
    idle = auto()
    launched = auto()
    diag_connected = auto()
    diag_disconnected = auto()
    ended = auto()


class Status(StrEnum):
    created = auto()
    started = auto()
    stopped = auto()


@dataclass
class Progress:
    phase: SessionPhase = SessionPhase.idle
    started_at: str | None = None
    ended_at: str | None = None
    events_count: int = 0
    game_states: list[str] = field(default_factory=list)
    steps: list[dict[str, str]] = field(default_factory=list)
    launcher_pid: int | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "phase": self.phase.value,
            "started_at": self.started_at,
            "ended_at": self.ended_at,
            "events_count": self.events_count,
            "game_states": list(self.game_states),
            "steps": list(self.steps),
            "launcher_pid": self.launcher_pid,
        }


class State:
    """Tracks one game session and background diagnostics pipe reader."""

    _running: bool = False
    status: Status
    run_id: str | None = None
    game_pid: int | None = None
    diag_thread: Thread
    diag_pipe: PipeServer
    progress: Progress
    events_file: TextIO | None = None
    run_dir_path: Path | None = None
    _stage_cond: Condition

    def __init__(self, diag_pipe: PipeServer):
        self.status = Status.created
        self.diag_pipe = diag_pipe
        self.progress = Progress()
        self._stage_cond = Condition()
        self.diag_thread = Thread(target=self.diag_thread_func, daemon=True)

    @property
    def current_stage(self) -> str | None:
        if not self.progress.game_states:
            return None
        return self.progress.game_states[-1]

    def record_step(self, name: str) -> None:
        self.progress.steps.append({"name": name, "at": _utc_now()})

    def start(self, launcher_pid: int) -> None:
        if self.game_pid is not None:
            raise RuntimeError("session already active")

        self._running = True
        self.status = Status.started
        self.run_id = new_run_id()
        self.game_pid = launcher_pid
        self.progress = Progress(
            phase=SessionPhase.launched,
            started_at=_utc_now(),
            launcher_pid=launcher_pid,
        )
        self.record_step("launch")

        self.run_dir_path = run_dir(self.run_id)
        events = events_path(self.run_id)
        self.events_file = events.open("w", encoding="utf-8")

        self.diag_thread = Thread(target=self.diag_thread_func, daemon=True)
        self.diag_thread.start()

    def _on_stage(self, phase: str) -> None:
        if not phase:
            return
        with self._stage_cond:
            if self.progress.game_states and self.progress.game_states[-1] == phase:
                return
            self.progress.game_states.append(phase)
            self.record_step(f"stage:{phase}")
            print(f"[stage] {phase} run_id={self.run_id}")
            self._stage_cond.notify_all()

    def wait_for_stage(self, stage: str, timeout: float) -> bool:
        with self._stage_cond:
            if stage in self.progress.game_states:
                return True
            if timeout <= 0:
                return False
            deadline = monotonic() + timeout
            while stage not in self.progress.game_states:
                remaining = deadline - monotonic()
                if remaining <= 0:
                    return False
                self._stage_cond.wait(timeout=remaining)
            return True

    def diag_thread_func(self) -> None:
        while self._running and not self.diag_pipe.accept():
            sleep(0.1)

        if not self._running:
            return

        with self._stage_cond:
            self.progress.phase = SessionPhase.diag_connected
        self.record_step("diag_connected")
        print(f"[stage] diag_connected run_id={self.run_id}")

        assert self.events_file is not None

        while self._running:
            try:
                line = self.diag_pipe.read_line()
                if line is None:
                    break
                self.events_file.write(line + "\n")
                self.events_file.flush()
                self.progress.events_count += 1
                try:
                    event = json.loads(line)
                    if event.get("type") == "game_state":
                        self._on_stage(str(event.get("phase", "")))
                except json.JSONDecodeError:
                    pass
            except Exception as e:
                print(f"[daemon] diagnostics read error: {e}")
                break

        try:
            self.diag_pipe.disconnect()
        except Exception:
            pass

        with self._stage_cond:
            self.progress.phase = SessionPhase.diag_disconnected
        self.record_step("diag_disconnected")
        print(f"[stage] diag_disconnected run_id={self.run_id}")

    def end_session(self) -> None:
        """Stop diagnostics reader, finalize meta; keep run_id for copy_logs."""
        self._running = False
        self.status = Status.stopped
        self.game_pid = None

        if self.diag_thread.is_alive():
            self.diag_thread.join(timeout=5.0)

        if self.events_file is not None:
            self.events_file.close()
            self.events_file = None

        self.progress.phase = SessionPhase.ended
        self.progress.ended_at = _utc_now()
        self._write_meta()
        if self.run_id and self.run_dir_path:
            write_last_run(self.run_id, self.run_dir_path)

        with self._stage_cond:
            self._stage_cond.notify_all()

    def _write_meta(self) -> None:
        if not self.run_id:
            return
        meta = {
            "run_id": self.run_id,
            "run_dir": str(self.run_dir_path.resolve()) if self.run_dir_path else None,
            "started_at": self.progress.started_at,
            "ended_at": self.progress.ended_at,
            "events": self.progress.events_count,
            "events_file": str(events_path(self.run_id).resolve()),
            "game_states": list(self.progress.game_states),
            "progress": self.progress.to_dict(),
            "launcher_pid": self.progress.launcher_pid,
        }
        meta_path(self.run_id).write_text(
            json.dumps(meta, indent=2) + "\n", encoding="utf-8"
        )


class CommandError(Exception):
    pass
