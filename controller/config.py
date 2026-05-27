from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict

REPO_ROOT = Path(__file__).resolve().parents[1]


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file="ctl.env")

    ctl_pipe_name: str = "thegame-ctl"
    diagnostics_pipe_name: str = "thegame-diagnostics"

    dll_debug_path: Path = (
        REPO_ROOT / "build" / "msvc-x86-debug" / "bin" / "TheGame.dll"
    )
    dll_release_path: Path = (
        REPO_ROOT / "build" / "msvc-x86-release" / "bin" / "TheGame.dll"
    )

    @property
    def dll_configs(self) -> dict[str, Path]:
        return {
            "debug": self.dll_debug_path,
            "release": self.dll_release_path,
        }
