#!/usr/bin/env python3
"""
Elevated launch control daemon for FA-EMU automation.

One-time (elevated):
  gsudo python launch/ctl/thegame_ctl.py -d

Non-elevated:
  python launch/ctl/thegame_ctl.py ping
  python launch/ctl/thegame_ctl.py diagnostics-run
  python launch/ctl/thegame_ctl.py kill --all
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

# Ensure launch/ is importable for launch_game
_CTL_DIR = Path(__file__).resolve().parent
_LAUNCH_DIR = _CTL_DIR.parent
_REPO_ROOT = _LAUNCH_DIR.parent
if str(_LAUNCH_DIR) not in sys.path:
    sys.path.insert(0, str(_LAUNCH_DIR))
if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

import launch_game  # noqa: E402

if str(_CTL_DIR) not in sys.path:
    sys.path.insert(0, str(_CTL_DIR))

import client  # noqa: E402
import paths  # noqa: E402
from client import DaemonNotRunningError  # noqa: E402
from server import run_daemon  # noqa: E402


def run_daemon_background() -> int:
    script = str(Path(__file__).resolve())
    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NO_WINDOW
    proc = subprocess.Popen(
        [sys.executable, script, "-d", "--foreground"],
        cwd=str(_REPO_ROOT),
        creationflags=creationflags,
    )
    print(f"daemon started in background pid={proc.pid}")
    print(f"log={paths.DAEMON_LOG_FILE}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="FA-EMU elevated control daemon (start with -d via gsudo).",
    )
    parser.add_argument(
        "-d",
        "--daemon",
        action="store_true",
        help="Run elevated daemon server (use with gsudo).",
    )
    parser.add_argument(
        "--foreground",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--background",
        action="store_true",
        help="With -d: detach daemon process.",
    )

    sub = parser.add_subparsers(dest="command")

    sub.add_parser("ping", help="Check daemon is running and elevated.")

    sub.add_parser("status", help="List GAME.exe / GameLauncher.exe PIDs.")

    kill_p = sub.add_parser("kill", help="Force-kill game processes.")
    kill_p.add_argument(
        "--all",
        action="store_true",
        help="Kill GAME.exe and GameLauncher.exe.",
    )

    copy_dll_p = sub.add_parser("copy-dll", help="Copy TheGame.dll next to GAME.exe.")
    copy_dll_p.add_argument(
        "--dll-config",
        choices=sorted(paths.DLL_CONFIGS),
        default="debug",
    )
    copy_dll_p.add_argument("--dll-source", type=Path, default=None)
    copy_dll_p.add_argument(
        "-p",
        "--game-exe",
        type=Path,
        default=launch_game.GAME_PATH,
    )

    copy_logs_p = sub.add_parser(
        "copy-logs",
        help="Copy shipping logs.txt/netlogs.txt into run dir.",
    )
    copy_logs_p.add_argument("--run-id", default=None)
    copy_logs_p.add_argument(
        "-p",
        "--game-exe",
        type=Path,
        default=launch_game.GAME_PATH,
    )

    sub.add_parser("stop", help="Stop the daemon.")

    diag_p = sub.add_parser(
        "diagnostics-run",
        help="Copy DLL, launch game, collect diagnostics into logs/runs/<run_id>/.",
    )
    diag_p.add_argument(
        "-p",
        "--game-exe",
        type=Path,
        default=launch_game.GAME_PATH,
    )
    diag_p.add_argument("-s", "--server-ip", default=None)
    diag_p.add_argument("--session-token", default=None)
    diag_p.add_argument("--username", default=None)
    diag_p.add_argument("--password", default=None)
    diag_p.add_argument(
        "--run-id", default=None, help="Run folder name under logs/runs/"
    )
    diag_p.add_argument(
        "--run-dir", type=Path, default=None, help="Advanced run directory override."
    )
    diag_p.add_argument(
        "--dll-config",
        choices=sorted(paths.DLL_CONFIGS),
        default="debug",
    )
    diag_p.add_argument("--dll-source", type=Path, default=None)
    diag_p.add_argument("--no-copy-dll", action="store_true")
    diag_p.add_argument("--no-collect-game-logs", action="store_true")

    return parser


def main(argv: list[str] | None = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.daemon:
        if args.background and not args.foreground:
            return run_daemon_background()
        return run_daemon(background=args.background)

    if not args.command:
        parser.print_help()
        return 0

    try:
        if args.command == "ping":
            return client.cmd_ping()
        if args.command == "status":
            return client.cmd_status()
        if args.command == "kill":
            return client.cmd_kill(all_games=args.all)
        if args.command == "copy-dll":
            return client.cmd_copy_dll(
                game_exe=args.game_exe.resolve(),
                dll_config=args.dll_config,
                dll_source=args.dll_source,
            )
        if args.command == "copy-logs":
            return client.cmd_copy_logs(
                run_id=args.run_id,
                game_exe=args.game_exe.resolve(),
            )
        if args.command == "stop":
            return client.cmd_stop()
        if args.command == "diagnostics-run":
            return client.cmd_diagnostics_run(
                game_exe=args.game_exe.resolve(),
                server_ip=args.server_ip,
                session_token=args.session_token,
                username=args.username,
                password=args.password,
                run_id=args.run_id,
                run_dir=args.run_dir,
                dll_config=args.dll_config,
                dll_source=args.dll_source,
                no_copy_dll=args.no_copy_dll,
                no_collect_game_logs=args.no_collect_game_logs,
            )
    except DaemonNotRunningError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except (RuntimeError, ValueError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
