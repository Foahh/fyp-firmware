#!/usr/bin/env python3

import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PROTO_DIR = REPO_ROOT / "Appli" / "Proto"
if str(PROTO_DIR) not in sys.path:
    sys.path.insert(0, str(PROTO_DIR))

import messages_pb2  # noqa: E402

try:
    from .visualizer_ports import resolve_port  # noqa: E402
    from .visualizer_serial import SerialLink  # noqa: E402
except ImportError:
    from visualizer_ports import resolve_port  # noqa: E402
    from visualizer_serial import SerialLink  # noqa: E402


def cmd_tracex_dump(
    project_root,
    port=None,
    output="tracex_dump.bin",
    chunk_size=256,
    baud=921600,
    timeout=2.0,
):
    del project_root
    selected_port = resolve_port(port)
    out_path = Path(output).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    link = SerialLink(selected_port, baud, timeout)
    try:
        req = messages_pb2.HostMessage()
        req.get_tracex_dump.chunk_size_bytes = int(chunk_size)
        req.get_tracex_dump.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
        link.send_host_message(req)

        total_size = None
        received = 0
        done = False
        deadline = time.time() + 30.0
        with open(out_path, "wb") as f:
            while time.time() < deadline:
                msg = link.recv_device_message(messages_pb2)
                which = msg.WhichOneof("payload")
                if which == "tracex_chunk":
                    chunk = msg.tracex_chunk
                    if total_size is None:
                        total_size = int(chunk.total_size_bytes)
                        print(f"[tracex] total_size={total_size} bytes")
                    f.seek(int(chunk.offset_bytes))
                    f.write(chunk.data)
                    received += len(chunk.data)
                    if chunk.done:
                        done = True
                elif which == "ack":
                    print(f"[tracex] ack success={msg.ack.success}")
                    if done:
                        break
                else:
                    continue

        if not done:
            raise RuntimeError(
                "TraceX dump did not complete (timeout or no done chunk)"
            )
        print(f"[tracex] wrote {received} bytes to {out_path}")
    finally:
        link.close()
