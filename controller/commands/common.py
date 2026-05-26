from config import Settings
from pydantic import BaseModel

from .state import State


class Command(BaseModel):
    command: str

    def invoke(self, settings: Settings, state: State) -> str:
        raise NotImplementedError("Subclasses must implement this method")
