#!/usr/bin/env python3
"""
Minimal FA-EMU launch automation.

Fetches a fresh launch token via GET /auth/launch (same as the Electron app),
writes config.ini, spawns GameLauncher.exe. No patch download or hash checks.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

# --- edit these ---
LAUNCHER_ROOT = Path(r"C:\Users\Svyat\AppData\Local\Programs\fa-emu-launcher")
API_BASE = "https://inx.fa-emu.com/api/v1"
# Login session uuid (store.json "token"). Leave empty to read from store.json.
ACCOUNT_TOKEN = ""
# Optional login if store.json is missing or session expired.
ACCOUNT_USERNAME = ""
ACCOUNT_PASSWORD = ""
DEFAULT_SERVER_IP = "137.184.201.52"
LANGUAGE = "EN"
ENABLE_DISCORD_PRESENCE = False
ENABLE_EXCLUSIVE_FULLSCREEN = False
SHOW_INGAME_FPS_COUNTER = True
GAME_PATH = Path(r"G:\Games\FA\FA-EMU\Shipping\GAME.exe")

STORE_PATH = LAUNCHER_ROOT / "store.json"
CONFIG_PATH = LAUNCHER_ROOT / "resources" / "assets" / "launcher" / "config.ini"
GAME_LAUNCHER_EXE = (
    LAUNCHER_ROOT / "resources" / "assets" / "launcher" / "GameLauncher.exe"
)


def _api_request(
    method: str,
    path: str,
    *,
    headers: dict[str, str] | None = None,
    body: dict[str, Any] | None = None,
) -> tuple[int, Any]:
    url = f"{API_BASE}{path}"
    req_headers = dict(headers or {})
    data = None
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        req_headers.setdefault("Content-Type", "application/json")
    req = urllib.request.Request(url, data=data, headers=req_headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            raw = resp.read().decode("utf-8")
            return resp.status, json.loads(raw) if raw else None
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", errors="replace")
        try:
            payload = json.loads(raw) if raw else None
        except json.JSONDecodeError:
            payload = raw
        raise RuntimeError(f"{method} {path} failed: HTTP {e.code}: {payload}") from e


def load_account_token(cli_token: str | None) -> str:
    if cli_token:
        return cli_token.strip()
    if ACCOUNT_TOKEN:
        return ACCOUNT_TOKEN.strip()
    if not STORE_PATH.is_file():
        raise RuntimeError(
            f"no session: {STORE_PATH} missing. Log in via FA-EMU Launcher once, "
            "set ACCOUNT_TOKEN, or pass --session-token / --username + --password"
        )
    data = json.loads(STORE_PATH.read_text(encoding="utf-8"))
    token = data.get("token")
    if not token:
        raise RuntimeError(f'no "token" in {STORE_PATH}')
    return str(token).strip()


def login(username: str, password: str) -> str:
    status, data = _api_request(
        "POST",
        "/auth/login",
        body={"username": username, "passwd": password},
    )
    if status != 201:
        raise RuntimeError(f"login failed: HTTP {status}: {data}")
    if not isinstance(data, dict) or not data.get("uuid"):
        raise RuntimeError(f"login response missing uuid: {data}")
    return str(data["uuid"])


def fetch_launch_credentials(account_token: str) -> dict[str, Any]:
    status, data = _api_request(
        "GET",
        "/auth/launch",
        headers={"X-Access-Token": account_token},
    )
    if status != 201:
        raise RuntimeError(f"auth/launch failed: HTTP {status}: {data}")
    if not isinstance(data, dict) or not data.get("token"):
        raise RuntimeError(f"auth/launch response missing token: {data}")
    return data


def write_config(game_path: Path, launch_token: str, server_ip: str) -> None:
    lines = [
        "[Launcher]",
        "",
        f"game_path={game_path}",
        f"m_token={launch_token}",
        f"m_serverIp={server_ip}",
        f"m_lang={LANGUAGE}",
        f"m_enableDiscordPresence={ENABLE_DISCORD_PRESENCE or 0}",
        f"m_enableExclusiveFullscreen={ENABLE_EXCLUSIVE_FULLSCREEN or 0}",
        f"m_showFpsCounter={SHOW_INGAME_FPS_COUNTER or 0}",
    ]
    CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text("\r\n".join(lines) + "\r\n", encoding="utf-8")


def spawn_game_launcher(
    launch_token: str, kernel_check_disable: bool
) -> subprocess.Popen:
    args = [str(GAME_LAUNCHER_EXE), launch_token]
    if kernel_check_disable:
        args.append("kernel-check-disable")

    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NO_WINDOW

    return subprocess.Popen(
        args,
        cwd=str(LAUNCHER_ROOT),
        creationflags=creationflags,
    )


def resolve_server_ip(cli_server_ip: str | None, launch_data: dict[str, Any]) -> str:
    if cli_server_ip is not None:
        return cli_server_ip
    api_ip = launch_data.get("server_ip")
    if api_ip:
        return str(api_ip)
    return DEFAULT_SERVER_IP


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Launch FA-EMU: fetch launch token, write config.ini, spawn GameLauncher.exe.",
    )
    parser.add_argument(
        "-p",
        "--game_exe",
        type=Path,
        default=GAME_PATH,
        help="Full path to GAME.exe (written as game_path in config.ini)",
    )
    parser.add_argument(
        "-s",
        "--server-ip",
        default=None,
        help=f"Override emulator server IP (default: from API, else {DEFAULT_SERVER_IP})",
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
    args = parser.parse_args()

    if (args.username is None) != (args.password is None):
        print("error: --username and --password must be used together", file=sys.stderr)
        return 1

    game_exe = args.game_exe.resolve()
    if not game_exe.is_file():
        print(f"error: GAME.exe not found: {game_exe}", file=sys.stderr)
        return 1
    if not GAME_LAUNCHER_EXE.is_file():
        print(
            f"error: GameLauncher.exe not found: {GAME_LAUNCHER_EXE}", file=sys.stderr
        )
        return 1

    try:
        if args.username:
            account_token = login(args.username, args.password)
        else:
            account_token = load_account_token(args.session_token)

        launch_data = fetch_launch_credentials(account_token)
        launch_token = str(launch_data["token"])
        server_ip = resolve_server_ip(args.server_ip, launch_data)
        kernel_check_disable = launch_data.get("kernel_check_disable") is True

        write_config(game_exe, launch_token, server_ip)
        proc = spawn_game_launcher(launch_token, kernel_check_disable)
    except (RuntimeError, urllib.error.URLError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    print(
        f"config={CONFIG_PATH} game={game_exe} server={server_ip} "
        f"kernel_check_disable={kernel_check_disable} pid={proc.pid}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
