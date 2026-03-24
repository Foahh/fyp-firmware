#!/usr/bin/env python3

import os
import struct
import sys
import time
from pathlib import Path

import serial

REPO_ROOT = Path(__file__).resolve().parent.parent
PROTO_DIR = REPO_ROOT / "Appli" / "Proto"
if str(PROTO_DIR) not in sys.path:
    sys.path.insert(0, str(PROTO_DIR))

import messages_pb2  # noqa: E402
from .visualizer import resolve_port  # noqa: E402


class SerialLink:
    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = serial.Serial(port, baud, timeout=timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._rx_buf = bytearray()
        self._max_frame_len = 16 * 1024

    def close(self) -> None:
        self._ser.close()

    def recv_device_message(self) -> messages_pb2.DeviceMessage:
        while True:
            while len(self._rx_buf) < 4:
                chunk = self._ser.read(4 - len(self._rx_buf))
                if not chunk:
                    raise IOError("Serial read timeout")
                self._rx_buf.extend(chunk)

            prefix = bytes(self._rx_buf[:4])
            length = struct.unpack("<I", prefix)[0]
            if length == 0 or length > self._max_frame_len:
                del self._rx_buf[0]
                continue

            need = 4 + length
            while len(self._rx_buf) < need:
                chunk = self._ser.read(need - len(self._rx_buf))
                if not chunk:
                    raise IOError("Serial read timeout")
                self._rx_buf.extend(chunk)

            payload = bytes(self._rx_buf[4:need])
            msg = messages_pb2.DeviceMessage()
            try:
                msg.ParseFromString(payload)
            except Exception:
                del self._rx_buf[0]
                continue
            del self._rx_buf[:need]
            return msg

    def send_host_message(self, msg: messages_pb2.HostMessage) -> None:
        payload = msg.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload
        self._ser.write(frame)
        self._ser.flush()


def cmd_tracex_dump(
    project_root,
    port=None,
    output="tracex_dump.bin",
    chunk_size=256,
    baud=115200,
    timeout=2.0,
):
    del project_root
    selected_port = resolve_port(port)
    out_path = Path(output).resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    link = SerialLink(selected_port, baud, timeout)
    try:
        req = messages_pb2.HostMessage()
        req.command_id = 1
        req.get_tracex_dump.chunk_size = int(chunk_size)
        link.send_host_message(req)

        total_size = None
        received = 0
        done = False
        deadline = time.time() + 30.0
        with open(out_path, "wb") as f:
            while time.time() < deadline:
                msg = link.recv_device_message()
                which = msg.WhichOneof("payload")
                if which == "tracex_chunk":
                    chunk = msg.tracex_chunk
                    if total_size is None:
                        total_size = int(chunk.total_size)
                        print(f"[tracex] total_size={total_size} bytes")
                    f.seek(int(chunk.offset))
                    f.write(chunk.data)
                    received += len(chunk.data)
                    if chunk.done:
                        done = True
                elif which == "ack":
                    print(
                        f"[tracex] ack command_id={msg.ack.command_id} success={msg.ack.success}"
                    )
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
