"""Elevated control daemon server loop."""

from __future__ import annotations

import json
import os
import sys
import traceback
from typing import TextIO

import paths
from commands import HANDLERS, cmd_diagnostics_run
from pipe_io import (
    create_ctl_server_pipe,
    disconnect_pipe,
    kernel32,
    read_line,
    wait_for_ctl_client,
    write_line,
)
from protocol import encode_response, parse_line

_shutdown = False


def request_shutdown() -> None:
    global _shutdown
    _shutdown = True


def is_shutdown_requested() -> bool:
    return _shutdown


def write_pid() -> None:
    paths.CTL_DIR.mkdir(parents=True, exist_ok=True)
    paths.PID_FILE.write_text(str(os.getpid()), encoding="utf-8")


def clear_pid() -> None:
    if paths.PID_FILE.is_file():
        paths.PID_FILE.unlink()


def log_daemon(msg: str, log_file: TextIO | None = None) -> None:
    line = f"[daemon] {msg}\n"
    if log_file:
        log_file.write(line)
        log_file.flush()
    else:
        print(line, end="")


def handle_request(
    handle,
    request_line: str,
    *,
    log_file: TextIO | None = None,
) -> bool:
    """Handle one RPC. Returns False if daemon should exit."""
    global _shutdown

    try:
        req = parse_line(request_line)
    except json.JSONDecodeError as e:
        write_line(handle, encode_response(0, ok=False, error=f"invalid json: {e}"))
        return True

    req_id = int(req.get("id", 0))
    cmd = req.get("cmd", "")
    args = req.get("args") or {}

    if cmd == "stop":
        write_line(
            handle,
            encode_response(req_id, ok=True, result={"stopping": True}),
        )
        request_shutdown()
        return False

    if cmd == "diagnostics_run":
        try:

            def stream(line: str) -> None:
                write_line(handle, line)

            result = cmd_diagnostics_run(args, write_stream=stream, req_id=req_id)
            write_line(handle, encode_response(req_id, ok=True, result=result))
        except Exception as e:
            log_daemon(f"diagnostics_run error: {e}\n{traceback.format_exc()}", log_file)
            write_line(handle, encode_response(req_id, ok=False, error=str(e)))
        return True

    handler = HANDLERS.get(cmd)
    if not handler:
        write_line(handle, encode_response(req_id, ok=False, error=f"unknown cmd: {cmd}"))
        return True

    try:
        result = handler(args)
        write_line(handle, encode_response(req_id, ok=True, result=result))
    except Exception as e:
        log_daemon(f"{cmd} error: {e}", log_file)
        write_line(handle, encode_response(req_id, ok=False, error=str(e)))
    return True


def run_daemon(*, background: bool = False) -> int:
    paths.ensure_logs_dirs()
    write_pid()

    log_path = paths.DAEMON_LOG_FILE
    log_mode = "a"
    log_file = open(log_path, log_mode, encoding="utf-8")

    if not background:
        print(f"thegame-ctl daemon pid={os.getpid()} pipe=\\\\.\\pipe\\thegame-ctl")
        print(f"log={log_path}")
        print("Ctrl+C or: python launch/ctl/thegame_ctl.py stop")

    log_daemon(f"started pid={os.getpid()} elevated", log_file)

    try:
        while not _shutdown:
            server = create_ctl_server_pipe()
            try:
                wait_for_ctl_client(server)
                pending = b""
                while not _shutdown:
                    line, pending = read_line(server, pending)
                    if line is None:
                        break
                    if not handle_request(server, line, log_file=log_file):
                        break
            finally:
                try:
                    disconnect_pipe(server)
                except OSError:
                    pass
                kernel32.CloseHandle(server)
    except KeyboardInterrupt:
        request_shutdown()
    finally:
        log_daemon("stopped", log_file)
        log_file.close()
        clear_pid()

    return 0
