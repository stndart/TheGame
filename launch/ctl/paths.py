"""Unified log paths for launch/diagnostics sessions."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
LAUNCH_DIR = REPO_ROOT / "launch"
CTL_DIR = LAUNCH_DIR / "ctl"

LOGS_ROOT = REPO_ROOT / "logs"
CTL_LOGS_DIR = LOGS_ROOT / "ctl"
RUNS_DIR = LOGS_ROOT / "runs"

PID_FILE = CTL_DIR / ".thegame-ctl.pid"
LAST_RUN_FILE = CTL_LOGS_DIR / "last_run.json"
DAEMON_LOG_FILE = CTL_LOGS_DIR / "daemon.log"

EVENTS_FILE = "events.jsonl"
GAME_LOGS_FILE = "game_logs.txt"
GAME_NETLOGS_FILE = "game_netlogs.txt"
META_FILE = "meta.json"

SHIPPING_LOGS_FILE = "logs.txt"
SHIPPING_NETLOGS_FILE = "netlogs.txt"

DLL_CONFIGS = {
    "debug": REPO_ROOT / "build" / "msvc-x86-debug" / "bin" / "TheGame.dll",
    "release": REPO_ROOT / "build" / "msvc-x86-release" / "bin" / "TheGame.dll",
}


def new_run_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def ensure_logs_dirs() -> None:
    CTL_LOGS_DIR.mkdir(parents=True, exist_ok=True)
    RUNS_DIR.mkdir(parents=True, exist_ok=True)


def run_dir(run_id: str, *, create: bool = True) -> Path:
    path = RUNS_DIR / run_id
    if create:
        path.mkdir(parents=True, exist_ok=True)
    return path


def events_path(run_id: str) -> Path:
    return run_dir(run_id) / EVENTS_FILE


def write_last_run(run_id: str, run_dir_path: Path) -> None:
    ensure_logs_dirs()
    payload = {"run_id": run_id, "run_dir": str(run_dir_path.resolve())}
    LAST_RUN_FILE.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def read_last_run() -> dict | None:
    if not LAST_RUN_FILE.is_file():
        return None
    return json.loads(LAST_RUN_FILE.read_text(encoding="utf-8"))


def resolve_run_id(run_id: str | None) -> str:
    if run_id:
        return run_id
    last = read_last_run()
    if not last or not last.get("run_id"):
        raise ValueError(
            "no --run-id and no last run; run diagnostics-run first or pass --run-id"
        )
    return str(last["run_id"])
