from pydantic import TypeAdapter

from .launch import LaunchCommand
from .ping import PingCommand
from .state import State

command_adapter = TypeAdapter(LaunchCommand | PingCommand)

__all__ = ["command_adapter", "State"]
