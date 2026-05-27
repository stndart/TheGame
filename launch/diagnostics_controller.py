#!/usr/bin/env python3
"""
Launch the game and collect diagnostics events from TheGame.dll.

Requires the elevated daemon (one-time):
  gsudo python launch/ctl/thegame_ctl.py -d

Then run (non-elevated):
  python launch/diagnostics_controller.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

_LAUNCH_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _LAUNCH_DIR.parent
_CTL_DIR = _LAUNCH_DIR / "ctl"
for p in (_LAUNCH_DIR, _CTL_DIR, _REPO_ROOT):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))

import launch_game  # noqa: E402
import paths as ctl_paths  # noqa: E402
from client import DaemonNotRunningError, cmd_diagnostics_run  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Launch FA-EMU and collect TheGame.dll diagnostics events.",
    )
    parser.add_argument(
        "-p",
        "--game_exe",
        type=Path,
        default=launch_game.GAME_PATH,
        help="Full path to GAME.exe",
    )
    parser.add_argument(
        "-s",
        "--server-ip",
        default=None,
        help=f"Override emulator server IP (default: from API, else {launch_game.DEFAULT_SERVER_IP})",
    )
    parser.add_argument(
        "--session-token",
        default=None,
        help="Account session token (login uuid). Default: store.json next to launcher",
    )
    parser.add_argument(
        "--username", default=None, help="Login instead of store.json session"
    )
    parser.add_argument("--password", default=None, help="Password for --username")
    parser.add_argument(
        "--dll-config",
        choices=sorted(ctl_paths.DLL_CONFIGS),
        default="debug",
        help="Which built TheGame.dll to copy next to GAME.exe before launch",
    )
    parser.add_argument(
        "--dll-source",
        type=Path,
        default=None,
        help="Explicit TheGame.dll path. Overrides --dll-config.",
    )
    parser.add_argument(
        "--no-copy-dll",
        action="store_true",
        help="Do not copy TheGame.dll before launch.",
    )
    parser.add_argument(
        "--run-id",
        default=None,
        help="Run folder under logs/runs/ (default: new UTC timestamp)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Deprecated alias; use logs/runs/<run_id>/events.jsonl (auto).",
    )
    parser.add_argument(
        "--no-collect-game-logs",
        action="store_true",
        help="Do not copy shipping logs.txt/netlogs.txt into run dir after session.",
    )
    args = parser.parse_args()

    if args.output is not None:
        print(
            "warning: --output is ignored; events are written to logs/runs/<run_id>/events.jsonl",
            file=sys.stderr,
        )

    try:
        return cmd_diagnostics_run(
            game_exe=args.game_exe.resolve(),
            server_ip=args.server_ip,
            session_token=args.session_token,
            username=args.username,
            password=args.password,
            run_id=args.run_id,
            dll_config=args.dll_config,
            dll_source=args.dll_source,
            no_copy_dll=args.no_copy_dll,
            no_collect_game_logs=args.no_collect_game_logs,
        )
    except DaemonNotRunningError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
