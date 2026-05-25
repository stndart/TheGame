"""Windows named pipe helpers for ctl and game diagnostics."""

from __future__ import annotations

import ctypes
from ctypes import wintypes

CTL_PIPE_NAME = r"\\.\pipe\thegame-ctl"
DIAGNOSTICS_PIPE_NAME = r"\\.\pipe\thegame-diagnostics"

INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
ERROR_BROKEN_PIPE = 109
ERROR_PIPE_CONNECTED = 535
ERROR_PIPE_BUSY = 231

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
PIPE_ACCESS_DUPLEX = 0x00000003
PIPE_ACCESS_INBOUND = 0x00000001
PIPE_TYPE_BYTE = 0x00000000
PIPE_READMODE_BYTE = 0x00000000
PIPE_WAIT = 0x00000000
FILE_ATTRIBUTE_NORMAL = 0x00000080
OPEN_EXISTING = 3

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

kernel32.CreateNamedPipeW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPVOID,
]
kernel32.CreateNamedPipeW.restype = wintypes.HANDLE
kernel32.ConnectNamedPipe.argtypes = [wintypes.HANDLE, wintypes.LPVOID]
kernel32.ConnectNamedPipe.restype = wintypes.BOOL
kernel32.CreateFileW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPVOID,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.HANDLE,
]
kernel32.CreateFileW.restype = wintypes.HANDLE
kernel32.ReadFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
kernel32.ReadFile.restype = wintypes.BOOL
kernel32.WriteFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPCVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
kernel32.WriteFile.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL
kernel32.DisconnectNamedPipe.argtypes = [wintypes.HANDLE]
kernel32.DisconnectNamedPipe.restype = wintypes.BOOL
kernel32.FlushFileBuffers.argtypes = [wintypes.HANDLE]
kernel32.FlushFileBuffers.restype = wintypes.BOOL

advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)

SECURITY_DESCRIPTOR_MIN_LENGTH = 20
SECURITY_DESCRIPTOR_REVISION = 1


class SECURITY_ATTRIBUTES(ctypes.Structure):
    _fields_ = [
        ("nLength", wintypes.DWORD),
        ("lpSecurityDescriptor", wintypes.LPVOID),
        ("bInheritHandle", wintypes.BOOL),
    ]


advapi32.InitializeSecurityDescriptor.argtypes = [
    wintypes.LPVOID,
    wintypes.DWORD,
]
advapi32.InitializeSecurityDescriptor.restype = wintypes.BOOL
advapi32.SetSecurityDescriptorDacl.argtypes = [
    wintypes.LPVOID,
    wintypes.BOOL,
    wintypes.LPVOID,
    wintypes.BOOL,
]
advapi32.SetSecurityDescriptorDacl.restype = wintypes.BOOL

_ctl_pipe_sd = (ctypes.c_byte * SECURITY_DESCRIPTOR_MIN_LENGTH)()
_ctl_pipe_sa: SECURITY_ATTRIBUTES | None = None


def _ctl_pipe_security_attributes() -> SECURITY_ATTRIBUTES:
    """Allow non-elevated clients to talk to an elevated daemon (same user)."""
    global _ctl_pipe_sa
    if _ctl_pipe_sa is not None:
        return _ctl_pipe_sa

    if not advapi32.InitializeSecurityDescriptor(
        ctypes.byref(_ctl_pipe_sd), SECURITY_DESCRIPTOR_REVISION
    ):
        raise_last_error("InitializeSecurityDescriptor")
    # NULL DACL grants access to everyone.
    if not advapi32.SetSecurityDescriptorDacl(
        ctypes.byref(_ctl_pipe_sd), True, None, False
    ):
        raise_last_error("SetSecurityDescriptorDacl")

    sa = SECURITY_ATTRIBUTES()
    sa.nLength = ctypes.sizeof(SECURITY_ATTRIBUTES)
    sa.lpSecurityDescriptor = ctypes.cast(
        ctypes.byref(_ctl_pipe_sd), wintypes.LPVOID
    )
    sa.bInheritHandle = False
    _ctl_pipe_sa = sa
    return sa


def raise_last_error(action: str) -> None:
    err = ctypes.get_last_error()
    raise OSError(err, f"{action} failed with Win32 error {err}")


def create_ctl_server_pipe() -> wintypes.HANDLE:
    sa = _ctl_pipe_security_attributes()
    handle = kernel32.CreateNamedPipeW(
        CTL_PIPE_NAME,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        4,
        65536,
        65536,
        0,
        ctypes.byref(sa),
    )
    if handle == INVALID_HANDLE_VALUE:
        raise_last_error("CreateNamedPipeW")
    return handle


def connect_ctl_client(*, retries: int = 20, retry_ms: int = 50) -> wintypes.HANDLE:
    import time

    last_err = 0
    for _ in range(retries):
        handle = kernel32.CreateFileW(
            CTL_PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            None,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            None,
        )
        if handle != INVALID_HANDLE_VALUE:
            return handle
        last_err = ctypes.get_last_error()
        if last_err == ERROR_PIPE_BUSY:
            time.sleep(retry_ms / 1000.0)
            continue
        raise_last_error("CreateFileW")
    raise OSError(last_err, f"CreateFileW failed with Win32 error {last_err}")


def daemon_pid_running(pid_file=None) -> bool:
    if pid_file is None:
        from pathlib import Path

        pid_file = Path(__file__).resolve().parent / ".thegame-ctl.pid"
    if not pid_file.is_file():
        return False
    try:
        pid = int(pid_file.read_text(encoding="utf-8").strip())
    except (ValueError, OSError):
        return False
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    STILL_ACTIVE = 259
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return False
    try:
        exit_code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
            return False
        return exit_code.value == STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel32.OpenProcess.restype = wintypes.HANDLE
kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
kernel32.GetExitCodeProcess.restype = wintypes.BOOL


def wait_for_ctl_client(server: wintypes.HANDLE) -> None:
    if kernel32.ConnectNamedPipe(server, None):
        return
    err = ctypes.get_last_error()
    if err != ERROR_PIPE_CONNECTED:
        raise OSError(err, f"ConnectNamedPipe failed with Win32 error {err}")


def disconnect_pipe(handle: wintypes.HANDLE) -> None:
    kernel32.FlushFileBuffers(handle)
    kernel32.DisconnectNamedPipe(handle)


def read_line(handle: wintypes.HANDLE, pending: bytes) -> tuple[str | None, bytes]:
    buffer = ctypes.create_string_buffer(4096)
    while True:
        if b"\n" in pending:
            raw, pending = pending.split(b"\n", 1)
            line = raw.decode("utf-8", errors="replace").rstrip("\r")
            return (line if line else None, pending)

        bytes_read = wintypes.DWORD(0)
        ok = kernel32.ReadFile(
            handle, buffer, len(buffer), ctypes.byref(bytes_read), None
        )
        if not ok:
            err = ctypes.get_last_error()
            if err == ERROR_BROKEN_PIPE:
                if pending:
                    line = pending.decode("utf-8", errors="replace").rstrip("\r")
                    return (line if line else None, b"")
                return (None, b"")
            raise OSError(err, f"ReadFile failed with Win32 error {err}")
        if bytes_read.value == 0:
            if pending:
                line = pending.decode("utf-8", errors="replace").rstrip("\r")
                return (line if line else None, b"")
            return (None, b"")

        pending += buffer.raw[: bytes_read.value]


def write_bytes(handle: wintypes.HANDLE, data: bytes) -> None:
    if not data:
        return
    written = wintypes.DWORD(0)
    ok = kernel32.WriteFile(handle, data, len(data), ctypes.byref(written), None)
    if not ok:
        raise_last_error("WriteFile")


def write_line(handle: wintypes.HANDLE, line: str) -> None:
    write_bytes(handle, (line + "\n").encode("utf-8"))


def create_diagnostics_server_pipe() -> wintypes.HANDLE:
    handle = kernel32.CreateNamedPipeW(
        DIAGNOSTICS_PIPE_NAME,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        65536,
        65536,
        0,
        None,
    )
    if handle == INVALID_HANDLE_VALUE:
        raise_last_error("CreateNamedPipeW")
    return handle


def wait_for_diagnostics_client(handle: wintypes.HANDLE) -> None:
    if kernel32.ConnectNamedPipe(handle, None):
        return
    err = ctypes.get_last_error()
    if err != ERROR_PIPE_CONNECTED:
        raise OSError(err, f"ConnectNamedPipe failed with Win32 error {err}")
