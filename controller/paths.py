from pathlib import Path

from config import REPO_ROOT

LOGS_ROOT = REPO_ROOT / "logs"
RUNS_DIR = LOGS_ROOT / "runs"

GAME_LOGS_FILE = "game_logs.txt"
GAME_NETLOGS_FILE = "game_netlogs.txt"
SHIPPING_LOGS_FILE = "logs.txt"
SHIPPING_NETLOGS_FILE = "netlogs.txt"


def run_dir(run_id: str) -> Path:
    path = RUNS_DIR / run_id
    path.mkdir(parents=True, exist_ok=True)
    return path
