"""
Non-elevated CLI for the thegame-ctl daemon.

Start the daemon once (elevated):  ctl -d
Then:  ctl ping | launch | wait-for-stage main_menu | kill --all | copy-logs
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

from commands import (
    CopyDllCommand,
    CopyLogsCommand,
    KillCommand,
    LaunchCommand,
    ListStagesCommand,
    PingCommand,
    ProcessesCommand,
    StatusCommand,
    StopCommand,
    WaitForStageCommand,
)
from commands.common import Command
from config import REPO_ROOT, Settings
from launch_game import Settings as LaunchSettings
from pipe import PipeClient

DAEMON_HINT = (
    "daemon not running; start once with:\n  gsudo ctl -d\n  # or: just daemon-bg"
)


class DaemonNotRunningError(RuntimeError):
    pass


def rpc(command: Command, *, timeout: float = 30.0) -> dict:
    """Send one command to the daemon and return the parsed response body."""
    settings = Settings()
    try:
        client = PipeClient(settings.ctl_pipe_name, timeout=int(timeout))
    except TimeoutError as e:
        raise DaemonNotRunningError(DAEMON_HINT) from e

    try:
        client.write(command.model_dump_json())
        raw = client.read()
    finally:
        client.close()

    payload = json.loads(raw)
    if payload.get("status") == "error":
        raise RuntimeError(payload.get("error", "unknown error"))
    if payload.get("status") != "ok":
        raise RuntimeError(f"unexpected response: {payload}")
    return payload


def run_daemon_background() -> int:
    """Spawn detached daemon process."""
    script = Path(__file__).resolve()
    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NO_WINDOW
    proc = subprocess.Popen(
        [sys.executable, str(script), "-d", "--foreground"],
        cwd=str(REPO_ROOT),
        creationflags=creationflags,
    )
    print(f"daemon started in background pid={proc.pid}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="FA-EMU control client (requires elevated daemon: ctl -d).",
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
    sub.add_parser("processes", help="List GAME.exe / GameLauncher.exe PIDs.")
    sub.add_parser("status", help="Session run_id, stages, and progress.")
    sub.add_parser("stages", help="Known stages and stages seen this session.")
    sub.add_parser("stop", help="Stop the daemon.")

    wait_p = sub.add_parser(
        "wait-for-stage",
        help="Block until game reaches a diagnostics stage (or timeout).",
    )
    wait_p.add_argument("stage", help="game_state phase name, e.g. main_menu")
    wait_p.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="Seconds to wait (default: 120).",
    )

    launch_p = sub.add_parser("launch", help="Copy config and spawn GameLauncher.")
    launch_p.add_argument(
        "-p",
        "--game-exe",
        type=Path,
        default=None,
    )
    launch_p.add_argument("-s", "--server-ip", default=None)
    launch_p.add_argument(
        "--offline",
        action="store_true",
        help="Use localhost token (no API).",
    )

    kill_p = sub.add_parser("kill", help="Force-kill game processes.")
    kill_p.add_argument(
        "--all",
        action="store_true",
        help="Kill GAME.exe and GameLauncher.exe.",
    )

    copy_dll_p = sub.add_parser("copy-dll", help="Copy TheGame.dll next to GAME.exe.")
    copy_dll_p.add_argument(
        "--dll-config",
        choices=["debug", "release"],
        default="debug",
    )
    copy_dll_p.add_argument("--dll-source", type=Path, default=None)
    copy_dll_p.add_argument("-p", "--game-exe", type=Path, default=None)

    copy_logs_p = sub.add_parser(
        "copy-logs",
        help="Copy shipping logs.txt/netlogs.txt into run dir.",
    )
    copy_logs_p.add_argument("--run-id", default=None)
    copy_logs_p.add_argument("-p", "--game-exe", type=Path, default=None)

    return parser


def _default_game_exe() -> Path:
    return LaunchSettings().GAME_PATH


def main(argv: list[str] | None = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.daemon:
        if args.background and not args.foreground:
            return run_daemon_background()
        from ctl import Ctl

        Ctl(Settings()).run_daemon()
        return 0

    if not args.command:
        parser.print_help()
        return 0

    try:
        if args.command == "ping":
            result = rpc(PingCommand())
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "processes":
            result = rpc(ProcessesCommand())
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "status":
            result = rpc(StatusCommand())
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "stages":
            result = rpc(ListStagesCommand())
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "wait-for-stage":
            timeout = max(args.timeout, 0.0)
            result = rpc(
                WaitForStageCommand(stage=args.stage, timeout=timeout),
                timeout=timeout + 10.0,
            )
            print(json.dumps(result, indent=2))
            return 0 if result.get("reached") else 1

        if args.command == "launch":
            game = args.game_exe or _default_game_exe()
            cmd = LaunchCommand(
                game_exe=str(game.resolve()),
                server_ip=args.server_ip,
                offline=args.offline,
            )
            result = rpc(cmd, timeout=60.0)
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "kill":
            result = rpc(KillCommand(all=args.all))
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "copy-dll":
            game = args.game_exe or _default_game_exe()
            cmd = CopyDllCommand(
                dll_config=args.dll_config,
                dll_source=str(args.dll_source) if args.dll_source else None,
                game_exe=str(game.resolve()),
            )
            result = rpc(cmd)
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "copy-logs":
            game = args.game_exe or _default_game_exe()
            cmd = CopyLogsCommand(
                run_id=args.run_id,
                game_exe=str(game.resolve()),
            )
            result = rpc(cmd)
            print(json.dumps(result, indent=2))
            return 0

        if args.command == "stop":
            rpc(StopCommand())
            return 0

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
