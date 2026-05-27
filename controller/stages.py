"""Known game_state phases from TheGame.dll diagnostics (keep in sync with src/hooks)."""

from __future__ import annotations

# Order matches typical boot flow.
KNOWN_STAGES: tuple[str, ...] = (
    "started",
    "intro",
    "connecting_to_server",
    "shard_choice",
    "main_menu",
)


def is_known_stage(name: str) -> bool:
    return name in KNOWN_STAGES
