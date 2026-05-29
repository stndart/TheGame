#!/usr/bin/env python3
"""One-shot: move pn_* ProudNet files and rewrite symbols (run from repo root)."""
from __future__ import annotations

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MOVES: list[tuple[str, str]] = [
    ("include/game/net/pn_layout.hpp", "include/ProudNet/Layout.hpp"),
    ("include/game/net/pn_message_type.hpp", "include/ProudNet/MessageType.hpp"),
    ("include/game/net/pn_tcp_frame.hpp", "include/ProudNet/TcpLayerMessageExtractor.hpp"),
    ("include/game/net/pn_drain_recv.hpp", "include/ProudNet/DrainReceiveQueue.hpp"),
    ("include/game/net/pn_process_message.hpp", "include/ProudNet/ProcessProudNetLayer.hpp"),
    ("include/game/net/pn_tcp_trace.hpp", "include/ProudNet/TcpTrace.hpp"),
    ("include/game/net/pn_socket_error.hpp", "include/ProudNet/SocketError.hpp"),
    ("include/game/net/pn_growable.hpp", "include/ProudNet/GrowableBuffer.hpp"),
    ("include/game/net/pn_fast_socket.hpp", "include/ProudNet/FastSocket.hpp"),
    ("include/game/net/pn_connection.hpp", "include/ProudNet/ConnectionNode.hpp"),
    ("include/game/net/pn_select_fd.hpp", "include/ProudNet/SelectFd.hpp"),
    ("include/game/net/pn_recv_append.hpp", "include/ProudNet/RecvAppend.hpp"),
    ("include/game/net/pn_fast_send.hpp", "include/ProudNet/FastSend.hpp"),
    ("include/game/net/pn_upnp.hpp", "include/ProudNet/UpnpClient.hpp"),
    ("include/game/net/pn_select.hpp", "include/ProudNet/SelectContext.hpp"),
    ("src/game/net/pn_tcp_frame.cpp", "src/ProudNet/TcpLayerMessageExtractor.cpp"),
    ("src/game/net/pn_drain_recv.cpp", "src/ProudNet/DrainReceiveQueue.cpp"),
    ("src/game/net/pn_process_message.cpp", "src/ProudNet/ProcessProudNetLayer.cpp"),
    ("src/game/net/pn_fast_socket.cpp", "src/ProudNet/FastSocket.cpp"),
    ("src/game/net/pn_recv_append.cpp", "src/ProudNet/RecvAppend.cpp"),
    ("src/game/net/pn_tcp_trace.cpp", "src/ProudNet/TcpTrace.cpp"),
    ("src/game/net/pn_connection.cpp", "src/ProudNet/ConnectionNode.cpp"),
    ("src/game/net/pn_fast_send.cpp", "src/ProudNet/FastSend.cpp"),
    ("src/game/net/pn_growable.cpp", "src/ProudNet/GrowableBuffer.cpp"),
    ("src/game/net/pn_socket_error.cpp", "src/ProudNet/SocketError.cpp"),
    ("src/game/net/pn_upnp.cpp", "src/ProudNet/UpnpClient.cpp"),
    ("src/game/net/pn_select_fd.cpp", "src/ProudNet/SelectFd.cpp"),
    ("src/game/net/pn_select.cpp", "src/ProudNet/SelectContext.cpp"),
]

INCLUDE_MAP = {
    "game/net/pn_layout.hpp": "ProudNet/Layout.hpp",
    "game/net/pn_message_type.hpp": "ProudNet/MessageType.hpp",
    "game/net/pn_tcp_frame.hpp": "ProudNet/TcpLayerMessageExtractor.hpp",
    "game/net/pn_drain_recv.hpp": "ProudNet/DrainReceiveQueue.hpp",
    "game/net/pn_process_message.hpp": "ProudNet/ProcessProudNetLayer.hpp",
    "game/net/pn_tcp_trace.hpp": "ProudNet/TcpTrace.hpp",
    "game/net/pn_socket_error.hpp": "ProudNet/SocketError.hpp",
    "game/net/pn_growable.hpp": "ProudNet/GrowableBuffer.hpp",
    "game/net/pn_fast_socket.hpp": "ProudNet/FastSocket.hpp",
    "game/net/pn_connection.hpp": "ProudNet/ConnectionNode.hpp",
    "game/net/pn_select_fd.hpp": "ProudNet/SelectFd.hpp",
    "game/net/pn_recv_append.hpp": "ProudNet/RecvAppend.hpp",
    "game/net/pn_fast_send.hpp": "ProudNet/FastSend.hpp",
    "game/net/pn_upnp.hpp": "ProudNet/UpnpClient.hpp",
    "game/net/pn_select.hpp": "ProudNet/SelectContext.hpp",
}

SKIP_DIRS = {
    "build",
    ".git",
    "GITS-FA-emulation",
    "ctl",
    "server",
    "transcripts",
    "journals",
}


def is_vendor_proudnet(path: Path) -> bool:
    s = str(path).replace("\\", "/")
    return "/ProudNet/ProudNet/" in s or s.endswith("/ProudNet/ProudNet")


def should_touch(path: Path) -> bool:
    if path.is_relative_to(ROOT / "tools"):
        return path.name != "migrate_proudnet_layout.py"
    if is_vendor_proudnet(path):
        return False
    parts = path.parts
    for skip in SKIP_DIRS:
        if skip in parts:
            return False
    return path.suffix in {".cpp", ".hpp", ".h"}


def wrap_proud_types(text: str) -> str:
    """Qualify type names outside #include lines (avoid ProudNet/Proud::Foo.hpp)."""
    types = [
        "CFastSocket",
        "GrowableBuffer",
        "RecvBuffer",
        "CSelectContext",
        "UpnpClient",
        "ConnectionNode",
        "SendArm",
        "CTcpLayerMessageExtractor",
    ]
    out_lines = []
    for line in text.splitlines(keepends=True):
        if line.lstrip().startswith("#include"):
            out_lines.append(line)
            continue
        for t in types:
            line = re.sub(rf"(?<!Proud::)\b{t}\b", f"Proud::{t}", line)
        out_lines.append(line)
    text = "".join(out_lines)
    while "Proud::Proud::" in text:
        text = text.replace("Proud::Proud::", "Proud::")
    return text


def rewrite(text: str) -> str:
    for old, new in INCLUDE_MAP.items():
        text = text.replace(f'#include "{old}"', f'#include "{new}"')

    text = text.replace("pn_inject_note_c2s_send", "Rmi::NoteC2sSend")
    text = text.replace("pn_inject_pump_lobby", "Rmi::PumpLobby")
    text = text.replace("pn_inject_pump_room", "Rmi::PumpRoom")

    text = text.replace(
        "bool process_message_proudnet_layer(",
        "bool Proud::ProcessMessageProudNetLayer(",
    )
    text = text.replace(
        "return process_message_proudnet_layer(",
        "return Proud::ProcessMessageProudNetLayer(",
    )
    text = text.replace(
        "void drain_receive_queue_call_original(",
        "void Proud::DrainReceiveQueueCallOriginal(",
    )
    text = text.replace(
        "void drain_receive_queue(", "void Proud::DrainReceiveQueue("
    )

    text = text.replace("void socket_report_error(", "void Proud::SocketReportError(")
    text = text.replace(
        "Proud::SocketReportError(this", "Proud::SocketReportError(this"
    )

    text = text.replace("PNFastSocket", "CFastSocket")
    text = text.replace("PNGrowableBuffer", "GrowableBuffer")
    text = text.replace("PNRecvBuffer", "RecvBuffer")
    text = text.replace("PNSelectContext", "CSelectContext")
    text = text.replace("PNUpnpClient", "UpnpClient")
    text = text.replace("PNConnectionNode", "ConnectionNode")
    text = text.replace("PNSendArm", "SendArm")
    text = text.replace("PNTcpFrame", "CTcpLayerMessageExtractor")

    text = text.replace("namespace fast_socket", "namespace FastSocketLayout")
    text = text.replace("namespace select_ctx", "namespace SelectLayout")
    text = text.replace("} // namespace fast_socket", "} // namespace FastSocketLayout")
    text = text.replace("} // namespace select_ctx", "} // namespace SelectLayout")
    text = text.replace("pn::fast_socket::", "Proud::FastSocketLayout::")
    text = text.replace("pn::select_ctx::", "Proud::SelectLayout::")

    text = text.replace("namespace pn", "namespace Proud")
    text = text.replace("} // namespace pn", "} // namespace Proud")
    text = text.replace("pn::", "Proud::")

    text = text.replace("namespace PnTcpTrace", "namespace TcpTrace")
    text = text.replace("} // namespace PnTcpTrace", "} // namespace TcpTrace")
    text = text.replace("PnTcpTrace::", "Proud::TcpTrace::")

    text = text.replace("pn_fast_socket.hpp", "ProudNet/FastSocket.hpp")
    text = text.replace("pn_connection.hpp", "ProudNet/ConnectionNode.hpp")
    text = text.replace("pn_growable.hpp", "ProudNet/GrowableBuffer.hpp")

    return wrap_proud_types(text)


def rewrite_tree() -> None:
    for path in ROOT.rglob("*"):
        if not path.is_file() or not should_touch(path):
            continue
        try:
            raw = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        new = rewrite(raw)
        new = new.replace("Proud::socket_report_error", "Proud::SocketReportError")
        if new != raw:
            path.write_text(new, encoding="utf-8", newline="\n")


def main() -> None:
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "--rewrite-only":
        rewrite_tree()
        print("rewrite complete")
        return

    (ROOT / "include/ProudNet").mkdir(parents=True, exist_ok=True)
    (ROOT / "src/ProudNet").mkdir(parents=True, exist_ok=True)

    for src_rel, dst_rel in MOVES:
        src = ROOT / src_rel
        dst = ROOT / dst_rel
        if not src.exists():
            raise SystemExit(f"missing source: {src_rel}")
        dst.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "mv", str(src), str(dst)], cwd=ROOT, check=True)

    rewrite_tree()

    remaining = list(ROOT.glob("**/pn_*"))
    remaining = [p for p in remaining if "ProudNet/ProudNet" not in str(p)]
    if remaining:
        print("WARN remaining pn_* paths:", remaining[:20])
    print(f"moved {len(MOVES)} files")


if __name__ == "__main__":
    main()
