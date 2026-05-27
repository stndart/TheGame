"""NDJSON RPC messages for thegame-ctl."""

from __future__ import annotations

import json
from typing import Any


def encode_request(req_id: int, cmd: str, args: dict[str, Any] | None = None) -> str:
    return json.dumps(
        {"id": req_id, "cmd": cmd, "args": args or {}}, separators=(",", ":")
    )


def encode_response(
    req_id: int,
    *,
    ok: bool,
    result: dict[str, Any] | None = None,
    error: str | None = None,
    msg_type: str | None = None,
    message: str | None = None,
    line: str | None = None,
) -> str:
    payload: dict[str, Any] = {"id": req_id}
    if msg_type:
        payload["type"] = msg_type
    if message is not None:
        payload["message"] = message
    if line is not None:
        payload["line"] = line
    if ok is not None and msg_type is None:
        payload["ok"] = ok
    if result is not None:
        payload["result"] = result
    if error is not None:
        payload["error"] = error
        payload["ok"] = False
    return json.dumps(payload, separators=(",", ":"))


def parse_line(line: str) -> dict[str, Any]:
    return json.loads(line)
