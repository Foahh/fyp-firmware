#!/usr/bin/env python3
"""Serial visualizer for Appli/Serial protobuf stream."""

from __future__ import annotations

import argparse
import os
import struct
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from queue import Empty, Queue
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np
import serial
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button
from serial.tools import list_ports


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PROTO_DIR = os.path.join(REPO_ROOT, "Appli", "Proto")
if PROTO_DIR not in sys.path:
    sys.path.insert(0, PROTO_DIR)

import messages_pb2  # noqa: E402


@dataclass
class VisualizerState:
    frame_count: int = 0
    last_timestamp: int = 0
    detection_count: int = 0
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
    overdrive_mode: bool = False
    camera_fps: int = 0
    mcu_freq_mhz: int = 0
    npu_freq_mhz: int = 0
    nn_size_text: str = "unknown"
    build_mode_text: str = "unknown"
    camera_mode_text: str = "unknown"
    build_timestamp: str = "unknown"
    display_enabled: Optional[bool] = None
    firmware_recognized: bool = False
    host_introduced: bool = False

    frame_drops: int = 0
    cpu_usage_percent: float = 0.0
    cpu_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))

    infer_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    post_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    period_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    fps_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    power_hist_mw: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    sync_hist: deque[bool] = field(default_factory=lambda: deque(maxlen=240))
    sync_period_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))

    hand_mm: int = 0
    hazard_mm: int = 0
    distance_3d_mm: float = 0.0
    tof_alert: bool = False
    tof_stale: bool = True
    tof_grid: np.ndarray = field(
        default_factory=lambda: np.full((8, 8), np.nan, dtype=np.float32)
    )

    last_ack: str = "No ACK received."
    last_error: str = ""
    last_power_error: str = ""
    last_rx_time: float = 0.0
    firmware_connected: bool = False
    power_connected: bool = False
    power_last_ts_us: int = 0
    power_current_ma: float = 0.0
    power_bus_v: float = 0.0
    power_mw: float = 0.0
    power_sync: bool = False
    power_sync_avg_mw: float = 0.0
    power_sync_peak_mw: float = 0.0
    power_sync_samples: int = 0


def read_exact(ser: serial.Serial, size: int) -> bytes:
    data = b""
    while len(data) < size:
        chunk = ser.read(size - len(data))
        if not chunk:
            raise IOError("Serial read timeout")
        data += chunk
    return data


class SerialLink:
    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = serial.Serial(port, baud, timeout=timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._tx_lock = threading.Lock()
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
            except Exception as exc:
                del self._rx_buf[0]
                continue

            del self._rx_buf[:need]
            return msg

    def send_host_message(self, msg: messages_pb2.HostMessage) -> None:
        payload = msg.SerializeToString()
        frame = struct.pack("<I", len(payload)) + payload
        with self._tx_lock:
            view = memoryview(frame)
            while view:
                written = self._ser.write(view)
                if written is None or written <= 0:
                    raise IOError("Serial write failed")
                view = view[written:]
            self._ser.flush()


class PowerLink:
    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = serial.Serial(port, baud, timeout=timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._ser.write(b"START\n")
        self._ser.flush()

    def close(self) -> None:
        self._ser.close()

    def recv_power_sample(self) -> tuple[int, float, float, float, int]:
        while True:
            raw = self._ser.readline()
            if not raw:
                raise IOError("Power serial read timeout")
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            if line.startswith("#"):
                continue
            if line.lower().startswith("ts_us,"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) != 5:
                continue
            try:
                ts_us = int(parts[0])
                current_ma = float(parts[1])
                bus_v = float(parts[2])
                power_mw = float(parts[3])
                sync = int(parts[4])
            except ValueError:
                continue
            return ts_us, current_ma, bus_v, power_mw, sync


def build_detection_text(
    result: messages_pb2.DetectionResult, class_labels: list[str]
) -> str:
    if not result.detections:
        return "No detections."

    lines = []
    for idx, det in enumerate(result.detections):
        class_name = str(det.class_index)
        if 0 <= det.class_index < len(class_labels):
            class_name = class_labels[det.class_index]
        lines.append(
            f"#{idx} {class_name:>10}  conf={det.conf:0.2f}  "
            f"xywh=({det.x_center:0.2f}, {det.y_center:0.2f}, {det.width:0.2f}, {det.height:0.2f})"
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
    link: Optional[SerialLink] = None
    while not stop_evt.is_set():
        try:
            if link is None:
                selected_port = resolve_port(port)
                link = SerialLink(selected_port, baud, timeout_s)
                state.firmware_connected = True
                state.last_error = ""
                # Re-introduce host after every reconnect so firmware can resend metadata.
                cmd_queue.put(("get_info", None))

            try:
                command, value = cmd_queue.get_nowait()
                msg = messages_pb2.HostMessage()
                msg.command_id = command_id
                command_id += 1
                if command == "get_info":
                    msg.get_device_info.SetInParent()
                    state.host_introduced = True
                elif command == "set_display":
                    msg.set_display_enabled.enabled = bool(value)
                    state.host_introduced = True
                elif command == "set_debug_op":
                    msg.set_debug_op_enabled.enabled = bool(value)
                    state.host_introduced = True
                link.send_host_message(msg)
            except Empty:
                pass

            dev_msg = link.recv_device_message()
            which = dev_msg.WhichOneof("payload")
            state.last_rx_time = time.time()

            if which == "detection_result":
                result = dev_msg.detection_result
                state.frame_count += 1
                state.last_timestamp = result.timestamp
                state.detection_count = len(result.detections)
                state.detections_text = build_detection_text(result, state.class_labels)

                if result.HasField("timing"):
                    period_ms = float(result.timing.nn_period_ms)
                    fps = (1000.0 / period_ms) if period_ms > 0.0 else 0.0
                    state.infer_hist.append(float(result.timing.inference_ms))
                    state.post_hist.append(float(result.timing.postprocess_ms))
                    state.period_hist.append(period_ms)
                    state.fps_hist.append(fps)
                    state.frame_drops = int(result.timing.frame_drops)

                if result.HasField("cpu"):
                    state.cpu_usage_percent = float(result.cpu.usage_percent)
                    state.cpu_hist.append(state.cpu_usage_percent)

                if result.HasField("tof"):
                    tof = result.tof
                    state.hand_mm = int(tof.hand_distance_mm)
                    state.hazard_mm = int(tof.hazard_distance_mm)
                    state.distance_3d_mm = float(tof.distance_3d_mm)
                    state.tof_alert = bool(tof.alert)
                    state.tof_stale = bool(tof.stale)
                    if len(tof.depth_grid) == 64:
                        state.tof_grid = np.array(
                            tof.depth_grid, dtype=np.float32
                        ).reshape(8, 8)
                    else:
                        state.tof_grid = np.full((8, 8), np.nan, dtype=np.float32)

            elif which == "device_info":
                info = dev_msg.device_info
                state.model_name = info.model_name or "unknown"
                state.class_labels = list(info.class_labels)
                state.device_info_command_id = int(info.command_id)
                state.display_width = int(info.display_width)
                state.display_height = int(info.display_height)
                state.letterbox_width = int(info.letterbox_width)
                state.letterbox_height = int(info.letterbox_height)
                state.nn_width = int(info.nn_width)
                state.nn_height = int(info.nn_height)
                state.nn_input_size_bytes = int(info.nn_input_size_bytes)
                state.overdrive_mode = bool(info.overdrive_mode)
                state.camera_fps = int(info.camera_fps)
                state.mcu_freq_mhz = int(info.mcu_freq_mhz)
                state.npu_freq_mhz = int(info.npu_freq_mhz)
                if info.nn_width and info.nn_height:
                    state.nn_size_text = f"{info.nn_width}x{info.nn_height} ({info.nn_input_size_bytes} B)"
                else:
                    state.nn_size_text = "unknown"
                state.build_mode_text = (
                    "overdrive" if info.overdrive_mode else "nominal"
                )
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

        except Exception as exc:  # Broad catch keeps GUI alive while cable reconnects.
            state.last_error = str(exc)
            state.firmware_connected = False
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
    # Aggregates for power statistics while SYNC pin is HIGH.
    sync_high_power_sum_mw = 0.0
    sync_high_samples = 0
    sync_high_peak_mw = 0.0
    prev_sync = 0
    last_rise_ts_us: Optional[int] = None
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

            ts_us, current_ma, bus_v, power_mw, sync = power_link.recv_power_sample()
            if power_mw is None or not np.isfinite(power_mw):
                power_mw = 0.0
            state.power_last_ts_us = ts_us
            state.power_current_ma = current_ma
            state.power_bus_v = bus_v
            state.power_mw = power_mw
            state.power_sync = sync == 1
            state.power_hist_mw.append(power_mw)
            state.sync_hist.append(sync == 1)
            if sync == 1 and prev_sync == 0:
                if last_rise_ts_us is not None:
                    period_ms = (ts_us - last_rise_ts_us) / 1000.0
                    if period_ms > 0.0:
                        state.sync_period_hist.append(period_ms)
                last_rise_ts_us = ts_us
            prev_sync = sync
            if state.power_sync:
                sync_high_power_sum_mw += power_mw
                sync_high_samples += 1
                sync_high_peak_mw = max(sync_high_peak_mw, power_mw)
            state.power_sync_samples = sync_high_samples
            state.power_sync_avg_mw = (
                (sync_high_power_sum_mw / sync_high_samples)
                if sync_high_samples > 0
                else 0.0
            )
            state.power_sync_peak_mw = sync_high_peak_mw
        except Exception as exc:  # Keep GUI alive if power monitor disconnects.
            state.last_power_error = str(exc)
            state.power_connected = False
            if power_link is not None:
                try:
                    power_link.close()
                except Exception:
                    pass
                power_link = None
            time.sleep(0.5)


def create_gui(
    state: VisualizerState,
    cmd_queue: Queue[tuple[str, Optional[bool]]],
) -> tuple[plt.Figure, FuncAnimation]:
    plt.style.use("dark_background")
    fig = plt.figure("FYP Firmware Serial Visualizer", figsize=(14, 9))
    if hasattr(fig.canvas.manager, "set_window_title"):
        fig.canvas.manager.set_window_title("FYP Firmware Serial Visualizer")

    grid = fig.add_gridspec(
        5,
        8,
        height_ratios=[1, 1, 1, 1, 0.25],
        width_ratios=[1, 1, 1, 1, 1, 1, 1, 1],
        hspace=0.55,
        wspace=0.45,
    )

    ax_timing = fig.add_subplot(grid[0:2, 0:4])
    ax_cpu = fig.add_subplot(grid[0:2, 4:6])
    ax_tof = fig.add_subplot(grid[0:2, 6:8])
    ax_power = fig.add_subplot(grid[2:4, 0:4])
    ax_text = fig.add_subplot(grid[2:4, 4:8])
    ax_text.axis("off")

    ax_btn_info = fig.add_subplot(grid[4, 1:3])
    ax_btn_toggle = fig.add_subplot(grid[4, 3:5])
    ax_btn_cam_only = fig.add_subplot(grid[4, 5:7])

    btn_info = Button(
        ax_btn_info, "Get Device Info", color="#333333", hovercolor="#555555"
    )
    btn_toggle = Button(
        ax_btn_toggle, "Toggle Display", color="#333333", hovercolor="#555555"
    )
    btn_cam_only = Button(
        ax_btn_cam_only,
        "Toggle CAM Pipe",
        color="#333333",
        hovercolor="#555555",
    )
    btn_info.label.set_color("white")
    btn_toggle.label.set_color("white")
    btn_cam_only.label.set_color("white")

    # --- Timing plot ---
    (line_inf,) = ax_timing.plot(
        [], [], label="Inference", color="#00ff00", linewidth=2
    )
    (line_period,) = ax_timing.plot(
        [], [], label="NN Period", color="#ffaa00", linewidth=2
    )
    (line_sync_period,) = ax_timing.plot(
        [], [], label="SYNC Period", color="#ff66cc", linewidth=2
    )
    ax_timing.set_title("Performance Timing (ms)", fontsize=14, pad=10)
    ax_timing.set_xlabel("Recent Frames", fontsize=10)
    ax_timing.set_ylabel("Milliseconds", fontsize=10)
    ax_timing.legend(loc="upper left", framealpha=0.7)
    ax_timing.grid(True, linestyle="--", alpha=0.3)
    ax_timing.spines["top"].set_visible(False)
    ax_timing.spines["right"].set_visible(False)

    # --- ToF heatmap ---
    img = ax_tof.imshow(
        np.full((8, 8), np.nan),
        cmap="magma",
        interpolation="nearest",
        vmin=0,
        vmax=2000,
    )
    cbar = fig.colorbar(img, ax=ax_tof, fraction=0.046, pad=0.04)
    cbar.set_label("Distance (mm)", rotation=270, labelpad=15)
    ax_tof.set_title("ToF Depth Grid (8x8)", fontsize=14, pad=10)
    ax_tof.set_xticks(np.arange(-0.5, 8, 1), minor=True)
    ax_tof.set_yticks(np.arange(-0.5, 8, 1), minor=True)
    ax_tof.grid(which="minor", color="w", linestyle="-", linewidth=1, alpha=0.2)
    ax_tof.tick_params(which="minor", bottom=False, left=False)
    ax_tof.set_xticks([])
    ax_tof.set_yticks([])

    # --- Power plot ---
    (line_power,) = ax_power.plot([], [], label="Power", color="#ff4444", linewidth=2)
    line_sync_avg = ax_power.axhline(
        y=0,
        color="#00ff88",
        linestyle="--",
        linewidth=1.5,
        label="Sync Avg",
        visible=False,
    )
    ax_power.set_title("Power (mW)", fontsize=14, pad=10)
    ax_power.set_xlabel("Recent Samples", fontsize=10)
    ax_power.set_ylabel("mW", fontsize=10)
    ax_power.legend(loc="upper left", framealpha=0.7)
    ax_power.grid(True, linestyle="--", alpha=0.3)
    ax_power.spines["top"].set_visible(False)
    ax_power.spines["right"].set_visible(False)
    power_peak_text = ax_power.text(
        0.98,
        0.95,
        "",
        transform=ax_power.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color="#ffaa00",
    )

    # --- CPU line chart ---
    (line_cpu,) = ax_cpu.plot([], [], label="CPU %", color="#00ff88", linewidth=2)
    ax_cpu.set_ylim(0, 100)
    ax_cpu.set_title("CPU (%)", fontsize=12, pad=10)
    ax_cpu.set_xlabel("Recent Frames", fontsize=10)
    ax_cpu.set_ylabel("%", fontsize=10)
    ax_cpu.spines["top"].set_visible(False)
    ax_cpu.spines["right"].set_visible(False)
    ax_cpu.grid(True, linestyle="--", alpha=0.3)
    cpu_percent_text = ax_cpu.text(
        0.98,
        0.95,
        "0%",
        transform=ax_cpu.transAxes,
        ha="right",
        va="top",
        fontsize=14,
        fontweight="bold",
        color="white",
    )
    cpu_freq_text = ax_cpu.text(
        0.98,
        0.05,
        "",
        transform=ax_cpu.transAxes,
        ha="right",
        va="bottom",
        fontsize=9,
        color="#dddddd",
    )

    # --- Text box ---
    text_box = ax_text.text(
        0.0,
        1.0,
        "",
        va="top",
        ha="left",
        family="monospace",
        fontsize=8,
        color="#eeeeee",
    )

    fig.tight_layout(pad=0.8)
    fig.subplots_adjust(left=0.06, right=0.96, top=0.96, bottom=0.02, hspace=1)

    display_toggle_state = {"enabled": True, "cam_pipe_enabled": True}
    sync_fill_ref = [None]

    def on_get_info(_event) -> None:
        cmd_queue.put(("get_info", None))

    def on_toggle_display(_event) -> None:
        display_toggle_state["enabled"] = not display_toggle_state["enabled"]
        if not display_toggle_state["enabled"]:
            display_toggle_state["cam_pipe_enabled"] = False
        else:
            display_toggle_state["cam_pipe_enabled"] = True
        cmd_queue.put(("set_display", display_toggle_state["enabled"]))

    def on_toggle_camera_only(_event) -> None:
        if not display_toggle_state["enabled"]:
            return
        display_toggle_state["cam_pipe_enabled"] = not display_toggle_state[
            "cam_pipe_enabled"
        ]
        cmd_queue.put(("set_debug_op", display_toggle_state["cam_pipe_enabled"]))

    btn_info.on_clicked(on_get_info)
    btn_toggle.on_clicked(on_toggle_display)
    btn_cam_only.on_clicked(on_toggle_camera_only)
    fig._visualizer_widgets = (btn_info, btn_toggle, btn_cam_only)

    def update(_frame_idx):
        # --- Timing ---
        x = np.arange(len(state.infer_hist))
        line_inf.set_data(x, np.array(state.infer_hist))
        line_period.set_data(x, np.array(state.period_hist))
        x_sync = np.arange(len(state.sync_period_hist))
        line_sync_period.set_data(x_sync, np.array(state.sync_period_hist))
        ax_timing.relim()
        ax_timing.autoscale_view()

        # --- ToF ---
        img.set_data(state.tof_grid)

        # --- Power ---
        x_power = np.arange(len(state.power_hist_mw))
        line_power.set_data(x_power, np.array(state.power_hist_mw))

        if state.power_sync_avg_mw > 0:
            line_sync_avg.set_ydata([state.power_sync_avg_mw])
            line_sync_avg.set_visible(True)
        else:
            line_sync_avg.set_visible(False)
        power_peak_text.set_text(f"peak: {state.power_sync_peak_mw:.0f} mW")

        # Sync highlight spans on power plot
        if sync_fill_ref[0] is not None:
            sync_fill_ref[0].remove()
            sync_fill_ref[0] = None
        if len(state.sync_hist) > 0:
            sync_arr = np.array(list(state.sync_hist), dtype=bool)
            x_sh = np.arange(len(sync_arr))
            sync_fill_ref[0] = ax_power.fill_between(
                x_sh,
                0,
                1,
                where=sync_arr,
                transform=ax_power.get_xaxis_transform(),
                alpha=0.15,
                color="#4488ff",
                zorder=0,
            )

        ax_power.relim()
        ax_power.autoscale_view()

        # --- Sync background tint for timing and tof ---
        bg = "#0a0a20" if state.power_sync else "#000000"
        ax_timing.set_facecolor(bg)
        ax_tof.set_facecolor(bg)
        ax_power.set_facecolor(bg)
        ax_cpu.set_facecolor(bg)

        # --- CPU line chart ---
        pct = state.cpu_usage_percent
        mcu_mhz = int(state.mcu_freq_mhz)
        cpu_usage_mhz = (pct / 100.0) * float(mcu_mhz) if mcu_mhz > 0 else 0.0
        x_cpu = np.arange(len(state.cpu_hist))
        line_cpu.set_data(x_cpu, np.array(state.cpu_hist))
        ax_cpu.relim()
        ax_cpu.autoscale_view(scalex=True, scaley=False)
        cpu_percent_text.set_text(f"{pct:.1f}%")
        npu = int(state.npu_freq_mhz)
        if mcu_mhz > 0:
            cpu_freq_text.set_text(
                f"{cpu_usage_mhz:.0f}MHz · CPU@{mcu_mhz}MHz / NPU@{npu}MHz"
            )
        else:
            cpu_freq_text.set_text(f"CPU@{mcu_mhz}MHz / NPU@{npu}MHz")

        # --- Text ---
        status = "ALERT" if state.tof_alert else "OK"
        stale = "stale" if state.tof_stale else "fresh"
        class_labels = ", ".join(state.class_labels) or "-"
        recog_host_fw = "yes" if state.host_introduced else "no"
        recog_fw_host = "yes" if state.firmware_recognized else "no"
        detections_text = state.detections_text or "  -"
        last_pp = state.post_hist[-1] if state.post_hist else 0.0

        lines = [
            f"Frames: {state.frame_count}  Drops: {state.frame_drops}  TS: {state.last_timestamp}ms  PP: {last_pp:.1f}ms",
            f"Model: {state.model_name}  NN: {state.nn_size_text}",
            f"Display: {state.display_width}x{state.display_height}  LB: {state.letterbox_width}x{state.letterbox_height}",
            f"Cam: {state.camera_mode_text}  Mode: {state.build_mode_text}",
            f"Built: {state.build_timestamp}",
            "",
            f"STM32: {'yes' if state.firmware_connected else 'no'}  H->D:{recog_host_fw} D->H:{recog_fw_host}",
            f"Disp: {'on' if display_toggle_state['enabled'] else 'off'}  Pipe: {'on' if display_toggle_state['cam_pipe_enabled'] else 'off'}",
            f"ToF: hand={state.hand_mm}mm haz={state.hazard_mm}mm {status}({stale})",
            f"ESP32: {'yes' if state.power_connected else 'no'}  {state.power_current_ma:.1f}mA {state.power_bus_v:.3f}V",
            f"ACK: {state.last_ack}",
            f"Err: {state.last_error or '-'}",
            f"PwrErr: {state.last_power_error or '-'}",
            f"Labels: {class_labels}",
            "",
            f"Detections ({state.detection_count}): {detections_text}",
        ]
        summary = "\n".join(lines)
        text_box.set_text(summary)

        return (
            line_inf,
            line_period,
            line_sync_period,
            img,
            text_box,
            line_power,
            line_sync_avg,
            power_peak_text,
            line_cpu,
            cpu_percent_text,
            cpu_freq_text,
        )

    anim = FuncAnimation(fig, update, interval=120, blit=False, cache_frame_data=False)
    return fig, anim


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
        default=115200,
        help="Baud rate (default: 115200)",
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
        default=115200,
        help="Power monitor baud rate (default: 115200)",
    )
    return parser.parse_args()


def resolve_port(port: Optional[str]) -> str:
    if port:
        return port

    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found. Pass port explicitly.")

    st_vids = {0x0483}
    keyword_scores = {
        "stm32": 6,
        "stmicro": 6,
        "stlink": 5,
        "nucleo": 4,
        "discovery": 4,
        "virtual com": 3,
        "vcp": 3,
        "usb serial": 1,
    }

    def score_port(info) -> int:
        score = 0
        if info.vid in st_vids:
            score += 10
        text = " ".join(
            [
                (info.manufacturer or ""),
                (info.product or ""),
                (info.description or ""),
                (info.interface or ""),
            ]
        ).lower()
        for needle, weight in keyword_scores.items():
            if needle in text:
                score += weight
        return score

    ranked = sorted(
        ports,
        key=lambda p: (score_port(p), p.device),
        reverse=True,
    )
    selected_info = ranked[0]
    selected = selected_info.device
    selected_score = score_port(selected_info)
    if selected_score == 0 and len(ranked) > 1:
        candidates = ", ".join(p.device for p in ranked[:4])
        print(
            f"[visualizer] Could not confidently identify firmware port; choosing {selected} from: {candidates}"
        )
    else:
        print(
            f"[visualizer] Auto-selected firmware-like port: {selected} (score={selected_score})"
        )
    return selected


def main() -> int:
    args = parse_args()
    state = VisualizerState()
    cmd_queue: Queue[tuple[str, Optional[bool]]] = Queue()
    stop_evt = threading.Event()

    rx_thread = threading.Thread(
        target=receiver_loop,
        args=(args.port, args.baud, args.timeout, state, cmd_queue, stop_evt),
        daemon=True,
    )
    rx_thread.start()
    power_thread = threading.Thread(
        target=power_receiver_loop,
        args=(args.power_port, args.power_baud, args.timeout, state, stop_evt),
        daemon=True,
    )
    power_thread.start()

    # Handshake: host introduces itself by requesting device metadata.
    cmd_queue.put(("get_info", None))

    fig, _anim = create_gui(state, cmd_queue)
    try:
        plt.show()
    finally:
        stop_evt.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
