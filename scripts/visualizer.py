#!/usr/bin/env python3
"""Serial visualizer for Appli/Serial protobuf stream."""

from __future__ import annotations

import argparse
import os
import threading
from queue import Queue
from typing import Any

import matplotlib.pyplot as plt

try:
    from .visualizer_gui import create_gui
    from .visualizer_ports import resolve_port, resolve_power_port
    from .visualizer_proto import REPO_ROOT
    from .visualizer_recording import RecordingManager
    from .visualizer_runtime import power_receiver_loop, receiver_loop
    from .visualizer_serial import probe_power_monitor_pm_ping
    from .visualizer_state import VisualizerState
except ImportError:
    from visualizer_gui import create_gui
    from visualizer_ports import resolve_port, resolve_power_port
    from visualizer_proto import REPO_ROOT
    from visualizer_recording import RecordingManager
    from visualizer_runtime import power_receiver_loop, receiver_loop
    from visualizer_serial import probe_power_monitor_pm_ping
    from visualizer_state import VisualizerState


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Visualizer for Appli/Serial stream")
    parser.add_argument(
        "port",
        nargs="?",
        default=None,
        help="Serial port (optional; auto-detected if omitted)",
    )
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=921600,
        help="Baud rate (default: 921600)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=2.0,
        help="Serial read timeout in seconds (default: 2.0)",
    )
    parser.add_argument(
        "--power-port",
        default=None,
        help="ESP32C6 power monitor serial port (optional)",
    )
    parser.add_argument(
        "--power-baud",
        type=int,
        default=921600,
        help="Power monitor baud rate (default: 921600)",
    )
    parser.add_argument(
        "--battery-mah",
        type=float,
        default=820.0,
        help="Battery capacity for runtime estimate (mAh at --battery-v, default: 820)",
    )
    parser.add_argument(
        "--battery-v",
        type=float,
        default=5.0,
        help="Supply voltage (V) for mWh = mAh × V; STM32 rail default 5",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    state = VisualizerState(
        battery_capacity_mah=float(args.battery_mah),
        battery_supply_voltage_v=float(args.battery_v),
    )
    recorder = RecordingManager(os.path.join(REPO_ROOT, "visualizer_screenshots"))
    cmd_queue: Queue[tuple[str, Any]] = Queue()
    stop_evt = threading.Event()

    firmware_port_for_power_exclude = resolve_port(args.port)
    rx_thread = threading.Thread(
        target=receiver_loop,
        args=(
            args.port,
            args.baud,
            args.timeout,
            state,
            resolve_port,
            cmd_queue,
            stop_evt,
            recorder,
        ),
        daemon=True,
    )
    rx_thread.start()

    power_port = resolve_power_port(
        args.power_port,
        exclude=firmware_port_for_power_exclude,
        baud=args.power_baud,
        handshake_timeout_s=args.timeout,
        probe_fn=probe_power_monitor_pm_ping,
    )
    power_thread = threading.Thread(
        target=power_receiver_loop,
        args=(power_port, args.power_baud, args.timeout, state, stop_evt, recorder),
        daemon=True,
    )
    power_thread.start()

    cmd_queue.put(("get_info", None))
    _figures, _animations = create_gui(state, cmd_queue, recorder)
    try:
        plt.show()
    finally:
        stop_evt.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
