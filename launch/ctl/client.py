"""Non-elevated client for thegame-ctl daemon."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any

import paths
from pipe_io import (
    connect_ctl_client,
    daemon_pid_running,
    kernel32,
    read_line,
    write_line,
)
from protocol import encode_request, parse_line

_LAUNCH_DIR = paths.LAUNCH_DIR
if str(_LAUNCH_DIR) not in sys.path:
    sys.path.insert(0, str(_LAUNCH_DIR))
import launch_game  # noqa: E402

DAEMON_HINT = (
    "daemon not running; start once with:\n  gsudo python launch/ctl/thegame_ctl.py -d"
)


class DaemonNotRunningError(RuntimeError):
    pass


def rpc(
    req_id: int, cmd: str, args: dict[str, Any] | None = None
) -> list[dict[str, Any]]:
    if not daemon_pid_running(paths.PID_FILE):
        raise DaemonNotRunningError(DAEMON_HINT)

    handle = connect_ctl_client()
    messages: list[dict[str, Any]] = []
    try:
        write_line(handle, encode_request(req_id, cmd, args))
        pending = b""
        while True:
            line, pending = read_line(handle, pending)
            if line is None:
                break
            msg = parse_line(line)
            messages.append(msg)
            if msg.get("ok") is not None and msg.get("type") is None:
                break
            if msg.get("ok") is False:
                break
    finally:
        kernel32.CloseHandle(handle)
    return messages


def print_stream_messages(messages: list[dict[str, Any]]) -> dict[str, Any] | None:
    final: dict[str, Any] | None = None
    for msg in messages:
        msg_type = msg.get("type")
        if msg_type == "progress":
            print(msg.get("message", ""))
            continue
        if msg_type == "event":
            line = msg.get("line", "")
            try:
                event = json.loads(line)
                if event.get("type") == "game_state":
                    print(f"[game_state] {event.get('phase', '?')}")
                else:
                    print(f"[{event.get('type', 'event')}] {line}")
            except json.JSONDecodeError:
                print(line)
            continue
        if msg.get("ok") is False:
            print(f"error: {msg.get('error', 'unknown')}", file=sys.stderr)
            final = msg
            continue
        if msg.get("ok") is True and "result" in msg:
            final = msg
    return final


def fetch_launch_credentials_client(
    *,
    session_token: str | None = None,
    username: str | None = None,
    password: str | None = None,
    server_ip: str | None = None,
) -> dict[str, Any]:
    if (username is None) != (password is None):
        raise ValueError("--username and --password must be used together")
    if username:
        account_token = launch_game.login(username, password)
    else:
        account_token = launch_game.load_account_token(session_token)

    launch_data = launch_game.fetch_launch_credentials(account_token)
    launch_token = str(launch_data["token"])
    resolved_ip = launch_game.resolve_server_ip(server_ip, launch_data)
    return {
        "launch_token": launch_token,
        "server_ip": resolved_ip,
        "kernel_check_disable": launch_data.get("kernel_check_disable") is True,
    }


def cmd_ping() -> int:
    msgs = rpc(1, "ping")
    final = print_stream_messages(msgs)
    if not final or not final.get("ok"):
        return 1
    print(json.dumps(final["result"], indent=2))
    return 0


def cmd_status() -> int:
    msgs = rpc(1, "status")
    final = print_stream_messages(msgs)
    if not final or not final.get("ok"):
        return 1
    print(json.dumps(final["result"], indent=2))
    return 0


def cmd_kill(*, all_games: bool = False, images: list[str] | None = None) -> int:
    args: dict[str, Any] = {}
    if images:
        args["images"] = images
    elif all_games:
        args["images"] = ["GAME.exe", "GameLauncher.exe"]
    msgs = rpc(1, "kill", args)
    final = print_stream_messages(msgs)
    return 0 if final and final.get("ok") else 1


def cmd_copy_dll(
    *,
    game_exe: Path,
    dll_config: str = "debug",
    dll_source: Path | None = None,
) -> int:
    args: dict[str, Any] = {
        "game_exe": str(game_exe),
        "dll_config": dll_config,
    }
    if dll_source:
        args["dll_source"] = str(dll_source)
    msgs = rpc(1, "copy_dll", args)
    final = print_stream_messages(msgs)
    if final and final.get("ok"):
        print(json.dumps(final["result"], indent=2))
        return 0
    return 1


def cmd_copy_logs(*, run_id: str | None, game_exe: Path) -> int:
    args: dict[str, Any] = {"game_exe": str(game_exe)}
    if run_id:
        args["run_id"] = run_id
    msgs = rpc(1, "copy_logs", args)
    final = print_stream_messages(msgs)
    if final and final.get("ok"):
        print(json.dumps(final["result"], indent=2))
        return 0
    return 1


def cmd_stop() -> int:
    msgs = rpc(1, "stop")
    final = print_stream_messages(msgs)
    return 0 if final and final.get("ok") else 1


def cmd_diagnostics_run(
    *,
    game_exe: Path,
    server_ip: str | None = None,
    session_token: str | None = None,
    username: str | None = None,
    password: str | None = None,
    run_id: str | None = None,
    run_dir: Path | None = None,
    dll_config: str = "debug",
    dll_source: Path | None = None,
    no_copy_dll: bool = False,
    no_collect_game_logs: bool = False,
) -> int:
    creds = fetch_launch_credentials_client(
        session_token=session_token,
        username=username,
        password=password,
        server_ip=server_ip,
    )
    launch_game.write_config(game_exe, creds["launch_token"], creds["server_ip"])

    rid = run_id or paths.new_run_id()
    if run_dir:
        paths.run_dir(rid)  # ensure parent exists
        # run_dir override: events go to run_dir/events.jsonl
        # For simplicity, run_dir must match paths.run_dir(rid) or we use run_id only
        if run_dir.resolve() != paths.run_dir(rid, create=False).resolve():
            paths.RUNS_DIR.mkdir(parents=True, exist_ok=True)
            run_dir.mkdir(parents=True, exist_ok=True)

    args: dict[str, Any] = {
        "game_exe": str(game_exe),
        "launch_token": creds["launch_token"],
        "server_ip": creds["server_ip"],
        "kernel_check_disable": creds["kernel_check_disable"],
        "run_id": rid,
        "dll_config": dll_config,
        "no_copy_dll": no_copy_dll,
        "no_collect_game_logs": no_collect_game_logs,
    }
    if dll_source:
        args["dll_source"] = str(dll_source)

    print(f"run_id={rid} run_dir={paths.run_dir(rid)}")
    msgs = rpc(1, "diagnostics_run", args)
    final = print_stream_messages(msgs)
    if not final or not final.get("ok"):
        return 1
    result = final["result"]
    print(f"done: events={result.get('events')} dir={result.get('run_dir')}")
    return 0
