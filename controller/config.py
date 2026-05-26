from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file="ctl.env")

    ctl_pipe_name: str = "thegame-ctl"
    diagnostics_pipe_name: str = r"\\.\pipe\thegame-diagnostics"
