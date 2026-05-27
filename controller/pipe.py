from __future__ import annotations

from time import sleep, time
from typing import TYPE_CHECKING, cast

import win32file
import win32pipe
import win32security
from win32.lib.winerror import ERROR_BROKEN_PIPE, ERROR_PIPE_BUSY, ERROR_PIPE_LISTENING

if TYPE_CHECKING:
    import _win32typing  # pyright: ignore[reportMissingModuleSource]


def _pipe_name(name: str) -> str:
    return f"\\\\.\\pipe\\{name}"


def _pipe_security_attributes() -> _win32typing.PySECURITY_ATTRIBUTES:
    """Allow non-elevated clients to talk to an elevated daemon."""
    sd = win32security.SECURITY_DESCRIPTOR()
    sd.Initialize()
    sd.SetSecurityDescriptorDacl(1, None, 0)  # NULL DACL # type: ignore
    sa = win32security.SECURITY_ATTRIBUTES()
    sa.SECURITY_DESCRIPTOR = sd
    return sa


class PipeServer:
    """Named pipe server for ctl or diagnostics sessions."""

    pipe: int  # pipe handle
    _pending: bytes

    def __init__(self, name: str):
        self._pending = b""
        self.pipe = win32pipe.CreateNamedPipe(
            _pipe_name(name),
            win32pipe.PIPE_ACCESS_DUPLEX,
            win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_NOWAIT,
            4,
            65536,
            65536,
            0,
            _pipe_security_attributes(),
        )

    def __del__(self):
        self.force_disconnect()

    def accept(self) -> bool:
        try:
            win32pipe.ConnectNamedPipe(self.pipe)
        except win32pipe.error as e:
            if e.winerror == ERROR_PIPE_LISTENING:
                return False
            raise e
        return True

    def disconnect(self):
        # blocks until client reads it all
        try:
            win32file.FlushFileBuffers(self.pipe)
        except win32file.error as e:
            if e.winerror != ERROR_BROKEN_PIPE:
                raise e
        win32pipe.DisconnectNamedPipe(self.pipe)

    def force_disconnect(self):
        # does not wait for client to read. Client would fail afterwards
        try:
            win32pipe.DisconnectNamedPipe(self.pipe)
        except win32pipe.error:
            pass

    def write(self, data: bytes | str):
        if isinstance(data, str):
            data = data.encode("utf-8")
        win32file.WriteFile(self.pipe, data)

    def read(self) -> str:
        pending, message = win32file.ReadFile(self.pipe, 4096)
        message = cast(bytes, message)
        return message.decode("utf-8")

    def read_line(self) -> str | None:
        """Read one newline-delimited line; None on pipe close."""
        while True:
            if b"\n" in self._pending:
                raw, self._pending = self._pending.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                return line if line else None

            try:
                _hr, chunk = win32file.ReadFile(self.pipe, 4096)
            except win32file.error as e:
                if e.winerror == ERROR_BROKEN_PIPE:
                    if self._pending:
                        line = self._pending.decode("utf-8", errors="replace").rstrip(
                            "\r"
                        )
                        self._pending = b""
                        return line if line else None
                    return None
                raise

            chunk = cast(bytes, chunk)
            if not chunk:
                if self._pending:
                    line = self._pending.decode("utf-8", errors="replace").rstrip("\r")
                    self._pending = b""
                    return line if line else None
                return None
            self._pending += chunk


class PipeClient:
    pipe: _win32typing.PyHANDLE

    def __init__(self, name: str, timeout: int = 1):
        ts = time()
        while time() - ts < timeout:
            sleep(0.1)
            try:
                self.pipe = win32file.CreateFile(
                    _pipe_name(name),
                    win32file.GENERIC_WRITE | win32file.GENERIC_READ,
                    0,
                    None,
                    win32file.OPEN_EXISTING,
                    0,
                    None,
                )
                break
            except win32file.error as e:
                if e.winerror == ERROR_PIPE_BUSY:
                    continue
                raise e
        else:
            raise TimeoutError(
                f"Failed to connect to pipe {name} after {timeout} seconds"
            )

    def __del__(self):
        self.close()

    def write(self, data: bytes | str):
        if isinstance(data, str):
            data = data.encode("utf-8")
        win32file.WriteFile(self.pipe.handle, data)

    def read(self) -> str:
        _error, message = win32file.ReadFile(self.pipe.handle, 10000)
        message = cast(bytes, message).decode("utf-8")
        return message

    def close(self):
        if hasattr(self, "pipe"):
            win32file.CloseHandle(self.pipe.handle)
            delattr(self, "pipe")
