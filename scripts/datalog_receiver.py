#!/usr/bin/env python3
"""Receive and display protobuf-encoded diagnostic data via serial."""

import argparse
import struct
import sys
import os

sys.path.insert(
    0, os.path.join(os.path.dirname(__file__), "..", "Appli", "Proto", "nanopb")
)

import serial
import messages_pb2


def read_exact(ser, n):
    buf = b""
    while len(buf) < n:
        chunk = ser.read(n - len(buf))
        if not chunk:
            raise IOError("Serial read timeout")
        buf += chunk
    return buf


def main():
    parser = argparse.ArgumentParser(description="nanopb datalog receiver")
    parser.add_argument("port", help="Serial port")
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)",
    )
    args = parser.parse_args()

    print(f"Connecting to {args.port} @ {args.baud}...")
    ser = serial.Serial(args.port, args.baud, timeout=5)
    frame_count = 0

    while True:
        length_bytes = read_exact(ser, 4)
        length = struct.unpack("<I", length_bytes)[0]
        if length == 0 or length > 4096:
            continue

        payload = read_exact(ser, length)
        msg = messages_pb2.DatalogMessage()
        msg.ParseFromString(payload)

        if msg.HasField("detection_result"):
            df = msg.detection_result
            t = df.timing
            frame_count += 1
            period = t.nn_period_ms
            fps = 1000.0 / period if period > 0 else 0
            print(
                f"[{frame_count:>6}] t={df.timestamp}ms  "
                f"inference={t.inference_ms}ms  postprocess={t.postprocess_ms}ms  "
                f"period={period}ms  fps={fps:.1f}"
            )
            for i, det in enumerate(df.detections):
                print(
                    f"         det[{i}]: class={det.class_index} conf={det.conf:.3f} "
                    f"bbox=({det.x_center:.3f}, {det.y_center:.3f}, "
                    f"{det.width:.3f}, {det.height:.3f})"
                )


if __name__ == "__main__":
    main()
