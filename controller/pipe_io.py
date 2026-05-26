import win32file
import win32pipe
import win32security
from config import Settings


class ClientPipe:
    ctl_pipe: int  # pipe handle

    def __init__(self, settings: Settings):
        self.ctl_pipe = win32pipe.CreateNamedPipe(
            settings.ctl_pipe_name,
            win32pipe.PIPE_ACCESS_DUPLEX,
            win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
            4,
            65536,
            65536,
            0,
            win32security.SECURITY_ATTRIBUTES(),
        )

    def __del__(self):
        self.disconnect_ctl_client()

    def connect_ctl_client(self):
        return win32pipe.ConnectNamedPipe(self.ctl_pipe, None)

    def disconnect_ctl_client(self):
        return win32pipe.DisconnectNamedPipe(self.ctl_pipe)

    def read_ctl_client(self) -> str:
        _hr, data = win32file.ReadFile(self.ctl_pipe, 4096)
        return data

    def write_ctl_client(self, data: bytes):
        win32file.WriteFile(self.ctl_pipe, data)


class Pipe(ClientPipe):
    diag_pipe: int  # pipe handle

    def __init__(self, settings: Settings):
        super().__init__(settings)
        self.diag_pipe = win32pipe.CreateNamedPipe(
            settings.diagnostics_pipe_name,
            win32pipe.PIPE_ACCESS_INBOUND,
            win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
            1,
            65536,
            65536,
            0,
            win32security.SECURITY_ATTRIBUTES(),
        )

    def __del__(self):
        self.disconnect_diag_client()

    def connect_diag_client(self):
        return win32pipe.ConnectNamedPipe(self.diag_pipe, None)

    def disconnect_diag_client(self):
        return win32pipe.DisconnectNamedPipe(self.diag_pipe)

    def read_diag_client(self) -> str:
        _hr, data = win32file.ReadFile(self.diag_pipe, 4096)
        return data
