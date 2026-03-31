#!/usr/bin/env python3
"""Serial visualizer for Appli/Serial protobuf stream."""

from __future__ import annotations

import argparse
import os
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from queue import Empty, Queue
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PROTO_DIR = os.path.join(REPO_ROOT, "Appli", "Proto")
POWER_PROTO_DIR = os.path.join(REPO_ROOT, "External", "fyp-power-measure")
for _dir in (PROTO_DIR, POWER_PROTO_DIR):
    if _dir not in sys.path:
        sys.path.insert(0, _dir)

import messages_pb2  # noqa: E402
import power_sample_pb2  # noqa: E402

# TofAlert.flags (protobuf); keep in sync with Appli/Proto/messages.proto
TOF_PB_FLAG_ALERT = 1 << 0
TOF_PB_FLAG_STALE = 1 << 1

try:
    from .visualizer_gui import create_gui  # noqa: E402
    from .visualizer_ports import resolve_port, resolve_power_port  # noqa: E402
    from .visualizer_serial import PowerLink, SerialLink, probe_power_monitor_pm_ping  # noqa: E402
except ImportError:
    from visualizer_gui import create_gui  # noqa: E402
    from visualizer_ports import resolve_port, resolve_power_port  # noqa: E402
    from visualizer_serial import PowerLink, SerialLink, probe_power_monitor_pm_ping  # noqa: E402


@dataclass
class VisualizerState:
    frame_count: int = 0
    last_timestamp: int = 0
    detection_timestamp: int = 0
    detection_count: int = 0
    tracked_box_count: int = 0
    detections_text: str = "No detections yet."

    model_name: str = "unknown"
    class_labels: list[str] = field(default_factory=list)
    device_info_command_id: int = 0
    display_width: int = 0
    display_height: int = 0
    letterbox_width: int = 0
    letterbox_height: int = 0
    nn_width: int = 0
    nn_height: int = 0
    nn_input_size_bytes: int = 0
    camera_fps: int = 0
    mcu_freq_mhz: int = 0
    npu_freq_mhz: int = 0
    power_mode: int = 1
    nn_size_text: str = "unknown"
    build_mode_text: str = "unknown"
    camera_mode_text: str = "unknown"
    build_timestamp: str = "unknown"
    device_info_timestamp: int = 0
    firmware_recognized: bool = False
    host_introduced: bool = False

    frame_drops: int = 0
    cpu_usage_percent: float = 0.0
    cpu_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    cpu_time_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))

    infer_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    post_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    tracker_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    period_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    fps_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    timing_time_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))

    hand_mm: int = 0
    hazard_mm: int = 0
    distance_3d_mm: float = 0.0
    tof_alert: bool = False
    tof_stale: bool = True
    tof_grid: np.ndarray = field(
        default_factory=lambda: np.full((8, 8), np.nan, dtype=np.float32)
    )

    display_enabled: bool = True
    last_ack: str = "No ACK received."
    last_ack_timestamp: int = 0
    last_error: str = ""
    last_power_error: str = ""
    last_rx_time: float = 0.0
    firmware_connected: bool = False
    firmware_port_path: str = ""
    firmware_baud: int = 0
    power_connected: bool = False
    power_in_inference: bool = False
    power_timestamp_us: int = 0
    power_infer_avg_mw: float = 0.0
    power_idle_avg_mw: float = 0.0
    power_infer_energy_uj: float = 0.0
    power_infer_duration_ms: float = 0.0
    power_infer_hist_mw: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    power_infer_time_hist: deque[float] = field(
        default_factory=lambda: deque(maxlen=240)
    )
    power_idle_hist_mw: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    power_idle_time_hist: deque[float] = field(
        default_factory=lambda: deque(maxlen=240)
    )


def build_detection_text(
    result: messages_pb2.DetectionResult, class_labels: list[str]
) -> str:
    if not result.detections:
        return "No detections."

    lines = []
    for idx, det in enumerate(result.detections):
        cid = int(det.class_id)
        class_name = str(cid)
        if 0 <= cid < len(class_labels):
            class_name = class_labels[cid]
        lines.append(
            f"#{idx} {class_name:>10}  conf={det.score:0.2f}  "
            f"xywh=({det.x:0.2f}, {det.y:0.2f}, {det.w:0.2f}, {det.h:0.2f})"
        )
    return "\n".join(lines)


def receiver_loop(
    port: Optional[str],
    baud: int,
    timeout_s: float,
    state: VisualizerState,
    cmd_queue: Queue[tuple[str, Optional[bool]]],
    stop_evt: threading.Event,
) -> None:
    command_id = 1
    pending_display_cmds: dict[int, bool] = {}
    link: Optional[SerialLink] = None
    last_get_info_mono: float = 0.0
    GET_INFO_RESEND_S = 1.5

    def send_get_info(why: str) -> None:
        nonlocal command_id, last_get_info_mono
        assert link is not None
        msg = messages_pb2.HostMessage()
        msg.command_id = command_id
        command_id += 1
        msg.get_device_info.SetInParent()
        msg.get_device_info.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
        link.send_host_message(msg)
        state.host_introduced = True
        last_get_info_mono = time.monotonic()
        print(f"[firmware] Sent get_device_info ({why})", file=sys.stderr)

    while not stop_evt.is_set():
        try:
            if link is None:
                selected_port = resolve_port(port)
                link = SerialLink(selected_port, baud, timeout_s)
                state.firmware_connected = True
                state.firmware_port_path = selected_port
                state.firmware_baud = baud
                state.firmware_recognized = False
                state.host_introduced = False
                state.last_error = ""
                print(
                    f"[firmware] Opened {selected_port} @ {baud} baud (8N1, no handshake)",
                    file=sys.stderr,
                )
                send_get_info("initial handshake")

            while True:
                try:
                    command, value = cmd_queue.get_nowait()
                except Empty:
                    break
                msg = messages_pb2.HostMessage()
                msg.command_id = command_id
                command_id += 1
                if command == "get_info":
                    msg.get_device_info.SetInParent()
                    msg.get_device_info.timestamp_ms = (
                        int(time.time() * 1000) & 0xFFFFFFFF
                    )
                    state.host_introduced = True
                    last_get_info_mono = time.monotonic()
                elif command == "set_display":
                    msg.set_display_enabled.enabled = bool(value)
                    msg.set_display_enabled.timestamp_ms = (
                        int(time.time() * 1000) & 0xFFFFFFFF
                    )
                    state.host_introduced = True
                    pending_display_cmds[msg.command_id] = bool(value)
                link.send_host_message(msg)
                if command == "get_info":
                    print("[firmware] Sent get_device_info (queued)", file=sys.stderr)

            if not state.firmware_recognized:
                now = time.monotonic()
                if now - last_get_info_mono >= GET_INFO_RESEND_S:
                    send_get_info("retry until DeviceInfo received")

            dev_msg = link.recv_device_message(messages_pb2)
            which = dev_msg.WhichOneof("payload")
            state.last_rx_time = time.time()

            if which == "detection_result":
                result = dev_msg.detection_result
                state.frame_count += 1
                state.last_timestamp = int(result.sent_timestamp_ms)
                state.detection_timestamp = int(result.frame_timestamp_ms)
                state.detection_count = len(result.detections)
                state.tracked_box_count = len(result.tracks)
                state.detections_text = build_detection_text(result, state.class_labels)

                period_us = float(result.nn_period_us)
                fps = (1000000.0 / period_us) if period_us > 0.0 else 0.0
                now = time.time()
                state.infer_hist.append(float(result.inference_us))
                state.post_hist.append(float(result.postprocess_us))
                state.tracker_hist.append(float(result.tracker_us))
                state.period_hist.append(period_us)
                state.fps_hist.append(fps)
                state.timing_time_hist.append(now)
                state.frame_drops = int(result.frame_drop_count)

                state.cpu_usage_percent = float(result.cpu_usage_percent)
                state.cpu_hist.append(state.cpu_usage_percent)
                state.cpu_time_hist.append(now)

                if result.HasField("tof"):
                    tof = result.tof
                    state.hand_mm = int(tof.hand_mm)
                    state.hazard_mm = int(tof.hazard_mm)
                    state.distance_3d_mm = float(tof.distance_3d_mm)
                    fl = int(tof.flags)
                    state.tof_alert = bool(fl & TOF_PB_FLAG_ALERT)
                    state.tof_stale = bool(fl & TOF_PB_FLAG_STALE)
                    if len(tof.depth_mm) == 64:
                        state.tof_grid = np.array(
                            tof.depth_mm, dtype=np.float32
                        ).reshape(8, 8)
                    else:
                        state.tof_grid = np.full((8, 8), np.nan, dtype=np.float32)

            elif which == "device_info":
                info = dev_msg.device_info
                state.model_name = info.model_name or "unknown"
                state.class_labels = list(info.class_labels)
                state.device_info_command_id = int(info.command_id)
                state.device_info_timestamp = int(info.timestamp_ms)
                state.display_width = int(info.display_width_px)
                state.display_height = int(info.display_height_px)
                state.letterbox_width = int(info.letterbox_width_px)
                state.letterbox_height = int(info.letterbox_height_px)
                state.nn_width = int(info.nn_width_px)
                state.nn_height = int(info.nn_height_px)
                state.nn_input_size_bytes = int(info.nn_input_size_bytes)
                state.camera_fps = int(info.camera_fps)
                state.mcu_freq_mhz = int(info.mcu_freq_mhz)
                state.npu_freq_mhz = int(info.npu_freq_mhz)
                state.power_mode = int(info.power_mode)
                if info.nn_width_px and info.nn_height_px:
                    state.nn_size_text = (
                        f"{info.nn_width_px}x{info.nn_height_px} "
                        f"({info.nn_input_size_bytes} B)"
                    )
                else:
                    state.nn_size_text = "unknown"
                if info.mcu_freq_mhz >= 800:
                    state.build_mode_text = "overdrive"
                elif info.mcu_freq_mhz <= 400:
                    state.build_mode_text = "underdrive"
                else:
                    state.build_mode_text = "nominal"
                if info.camera_fps <= 0:
                    state.camera_mode_text = "snapshot"
                else:
                    state.camera_mode_text = f"{info.camera_fps} fps"
                state.firmware_recognized = True
                state.build_timestamp = info.build_timestamp or "unknown"

            elif which == "ack":
                ack = dev_msg.ack
                state.last_ack = (
                    f"ACK command_id={ack.command_id} success={ack.success}"
                )
                state.last_ack_timestamp = int(ack.timestamp_ms)
                if ack.command_id in pending_display_cmds:
                    if ack.success:
                        state.display_enabled = pending_display_cmds[ack.command_id]
                    del pending_display_cmds[ack.command_id]

        except Exception as exc:
            state.last_error = str(exc)
            state.firmware_connected = False
            state.firmware_port_path = ""
            state.firmware_baud = 0
            state.firmware_recognized = False
            state.host_introduced = False
            print(f"[firmware] Error: {exc}", file=sys.stderr)
            if link is not None:
                try:
                    link.close()
                except Exception:
                    pass
                link = None
            time.sleep(0.5)


def power_receiver_loop(
    power_port: Optional[str],
    power_baud: int,
    timeout_s: float,
    state: VisualizerState,
    stop_evt: threading.Event,
) -> None:
    power_link: Optional[PowerLink] = None
    while not stop_evt.is_set():
        try:
            if power_port is None:
                state.power_connected = False
                time.sleep(0.5)
                continue

            if power_link is None:
                power_link = PowerLink(power_port, power_baud, timeout_s)
                state.power_connected = True
                state.last_power_error = ""

            sample = power_link.recv_power_sample(power_sample_pb2)
            energy_j = float(sample.energy_j)
            duration_us = float(sample.duration_us)
            is_inference = bool(sample.is_inference)
            timestamp_us = int(sample.timestamp_us)

            state.power_in_inference = is_inference
            state.power_timestamp_us = timestamp_us
            now = time.time()
            duration_s = duration_us * 1e-6
            avg_mw = (energy_j / duration_s) * 1000.0 if duration_s > 0.0 else 0.0

            if is_inference:
                state.power_infer_hist_mw.append(avg_mw)
                state.power_infer_time_hist.append(now)
                state.power_infer_avg_mw = avg_mw
                state.power_infer_duration_ms = duration_us / 1000.0
                state.power_infer_energy_uj = energy_j * 1e6
            else:
                state.power_idle_hist_mw.append(avg_mw)
                state.power_idle_time_hist.append(now)
                state.power_idle_avg_mw = avg_mw
        except Exception as exc:
            state.last_power_error = str(exc)
            state.power_connected = False
            print(f"[power] Error: {exc}", file=sys.stderr)
            if power_link is not None:
                try:
                    power_link.close()
                except Exception:
                    pass
                power_link = None
            time.sleep(0.5)


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
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    state = VisualizerState()
    cmd_queue: Queue[tuple[str, Optional[bool]]] = Queue()
    stop_evt = threading.Event()

    firmware_port_for_power_exclude = resolve_port(args.port)
    rx_thread = threading.Thread(
        target=receiver_loop,
        args=(args.port, args.baud, args.timeout, state, cmd_queue, stop_evt),
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
        args=(power_port, args.power_baud, args.timeout, state, stop_evt),
        daemon=True,
    )
    power_thread.start()

    cmd_queue.put(("get_info", None))

    _figures, _animations = create_gui(state, cmd_queue)
    try:
        plt.show()
    finally:
        stop_evt.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
