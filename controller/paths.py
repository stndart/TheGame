"""Log and run directory paths for controller sessions."""

from __future__ import annotations

import json
from pathlib import Path

from config import REPO_ROOT

LOGS_ROOT = REPO_ROOT / "logs"
CTL_LOGS_DIR = LOGS_ROOT / "ctl"
RUNS_DIR = LOGS_ROOT / "runs"

LAST_RUN_FILE = CTL_LOGS_DIR / "last_run.json"

EVENTS_FILE = "events.jsonl"
META_FILE = "meta.json"
GAME_LOGS_FILE = "game_logs.txt"
GAME_NETLOGS_FILE = "game_netlogs.txt"
SHIPPING_LOGS_FILE = "logs.txt"
SHIPPING_NETLOGS_FILE = "netlogs.txt"


def ensure_logs_dirs() -> None:
    CTL_LOGS_DIR.mkdir(parents=True, exist_ok=True)
    RUNS_DIR.mkdir(parents=True, exist_ok=True)


def run_dir(run_id: str) -> Path:
    path = RUNS_DIR / run_id
    path.mkdir(parents=True, exist_ok=True)
    return path


def events_path(run_id: str) -> Path:
    return run_dir(run_id) / EVENTS_FILE


def meta_path(run_id: str) -> Path:
    return run_dir(run_id) / META_FILE


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
            "no run_id and no last run; launch a session first or pass run_id"
        )
    return str(last["run_id"])


def merge_meta(run_id: str, patch: dict) -> None:
    path = meta_path(run_id)
    meta: dict = {}
    if path.is_file():
        meta = json.loads(path.read_text(encoding="utf-8"))
    meta.update(patch)
    path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
