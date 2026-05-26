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

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env")

    LAUNCHER_ROOT: Path = Path(r"C:\Users\Svyat\AppData\Local\Programs\fa-emu-launcher")
    GAME_PATH: Path = Path(r"G:\Games\FA\FA-EMU\Shipping\GAME.exe")
    API_BASE: str = "https://inx.fa-emu.com/api/v1"
    DEFAULT_SERVER_IP: str = "137.184.201.52"

    # Login session uuid (store.json "token"). Leave empty to read from store.json.
    ACCOUNT_TOKEN: str = ""
    # Optional login if store.json is missing or session expired.
    ACCOUNT_USERNAME: str = ""
    ACCOUNT_PASSWORD: str = ""

    LANGUAGE: str = "EN"
    ENABLE_DISCORD_PRESENCE: bool = False
    ENABLE_EXCLUSIVE_FULLSCREEN: bool = False
    SHOW_INGAME_FPS_COUNTER: bool = True

    def get_store_path(self) -> Path:
        return self.LAUNCHER_ROOT / "store.json"

    def get_config_path(self) -> Path:
        return self.LAUNCHER_ROOT / "resources" / "assets" / "launcher" / "config.ini"

    def get_game_launcher_exe(self) -> Path:
        return (
            self.LAUNCHER_ROOT
            / "resources"
            / "assets"
            / "launcher"
            / "GameLauncher.exe"
        )


def _api_request(
    settings: Settings,
    method: str,
    path: str,
    *,
    headers: dict[str, str] | None = None,
    body: dict[str, Any] | None = None,
) -> tuple[int, Any]:
    url = f"{settings.API_BASE}{path}"
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


def load_account_token(settings: Settings, cli_token: str | None = None) -> str:
    if cli_token:
        return cli_token.strip()
    if settings.ACCOUNT_TOKEN:
        return settings.ACCOUNT_TOKEN.strip()
    if not settings.get_store_path().is_file():
        raise RuntimeError(
            f"no session: {settings.get_store_path()} missing. Log in via FA-EMU Launcher once, "
            "set ACCOUNT_TOKEN, or pass --session-token / --username + --password"
        )
    data = json.loads(settings.get_store_path().read_text(encoding="utf-8"))
    token = data.get("token")
    if not token:
        raise RuntimeError(f'no "token" in {settings.get_store_path()}')
    return str(token).strip()


def login(settings: Settings, username: str, password: str) -> str:
    status, data = _api_request(
        settings,
        "POST",
        "/auth/login",
        body={"username": username, "passwd": password},
    )
    if status != 201:
        raise RuntimeError(f"login failed: HTTP {status}: {data}")
    if not isinstance(data, dict) or not data.get("uuid"):
        raise RuntimeError(f"login response missing uuid: {data}")
    return str(data["uuid"])


def fetch_launch_credentials(settings: Settings, account_token: str) -> dict[str, Any]:
    status, data = _api_request(
        settings,
        "GET",
        "/auth/launch",
        headers={"X-Access-Token": account_token},
    )
    if status != 201:
        raise RuntimeError(f"auth/launch failed: HTTP {status}: {data}")
    if not isinstance(data, dict) or not data.get("token"):
        raise RuntimeError(f"auth/launch response missing token: {data}")
    return data


def write_config(
    settings: Settings, game_path: Path, launch_token: str, server_ip: str
) -> None:
    lines = [
        "[Launcher]",
        "",
        f"game_path={game_path}",
        f"m_token={launch_token}",
        f"m_serverIp={server_ip}",
        f"m_lang={settings.LANGUAGE}",
        f"m_enableDiscordPresence={settings.ENABLE_DISCORD_PRESENCE or 0}",
        f"m_enableExclusiveFullscreen={settings.ENABLE_EXCLUSIVE_FULLSCREEN or 0}",
        f"m_showFpsCounter={settings.SHOW_INGAME_FPS_COUNTER or 0}",
    ]
    settings.get_config_path().parent.mkdir(parents=True, exist_ok=True)
    settings.get_config_path().write_text("\r\n".join(lines) + "\r\n", encoding="utf-8")


def spawn_game_launcher(
    settings: Settings, launch_token: str, kernel_check_disable: bool
) -> subprocess.Popen:
    args = [str(settings.get_game_launcher_exe()), launch_token]
    if kernel_check_disable:
        args.append("kernel-check-disable")

    creationflags = 0
    if sys.platform == "win32":
        creationflags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NO_WINDOW

    return subprocess.Popen(
        args,
        cwd=str(settings.LAUNCHER_ROOT),
        creationflags=creationflags,
    )


def resolve_server_ip(
    settings: Settings, cli_server_ip: str | None, launch_data: dict[str, Any]
) -> str:
    if cli_server_ip is not None:
        return cli_server_ip
    api_ip = launch_data.get("server_ip")
    if api_ip:
        return str(api_ip)
    return settings.DEFAULT_SERVER_IP


def main() -> int:
    settings = Settings()

    parser = argparse.ArgumentParser(
        description="Launch FA-EMU: fetch launch token, write config.ini, spawn GameLauncher.exe.",
    )
    parser.add_argument(
        "-p",
        "--game_exe",
        type=Path,
        default=settings.GAME_PATH,
        help="Full path to GAME.exe (written as game_path in config.ini)",
    )
    parser.add_argument(
        "-s",
        "--server-ip",
        default=None,
        help=f"Override emulator server IP (default: from API, else {settings.DEFAULT_SERVER_IP})",
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
        "--offline",
        default=False,
        action="store_true",
        help="Run the game in localhost mode",
    )
    args = parser.parse_args()

    if (args.username is None) != (args.password is None):
        print("error: --username and --password must be used together", file=sys.stderr)
        return 1

    game_exe = args.game_exe.resolve()
    if not game_exe.is_file():
        print(f"error: GAME.exe not found: {game_exe}", file=sys.stderr)
        return 1
    if not settings.get_game_launcher_exe().is_file():
        print(
            f"error: GameLauncher.exe not found: {settings.get_game_launcher_exe()}",
            file=sys.stderr,
        )
        return 1

    try:
        if not args.offline:
            if args.username:
                account_token = login(settings, args.username, args.password)
            else:
                account_token = load_account_token(settings, args.session_token)

            launch_data = fetch_launch_credentials(settings, account_token)
            launch_token = str(launch_data["token"])
        else:
            launch_token = args.session_token or "localhost"

        server_ip = resolve_server_ip(settings, args.server_ip, launch_data)
        kernel_check_disable = launch_data.get("kernel_check_disable") is True

        write_config(settings, game_exe, launch_token, server_ip)
        proc = spawn_game_launcher(settings, launch_token, kernel_check_disable)
    except (RuntimeError, urllib.error.URLError, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    print(
        f"config={settings.get_config_path()} game={game_exe} server={server_ip} "
        f"kernel_check_disable={kernel_check_disable} pid={proc.pid}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
