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
POWER_PROTO_DIR = os.path.join(REPO_ROOT, "External", "fyp-power-measure")
for _dir in (PROTO_DIR, POWER_PROTO_DIR):
    if _dir not in sys.path:
        sys.path.insert(0, _dir)

import messages_pb2  # noqa: E402
import power_sample_pb2  # noqa: E402


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
    firmware_recognized: bool = False
    host_introduced: bool = False

    frame_drops: int = 0
    cpu_usage_percent: float = 0.0
    cpu_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    cpu_time_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))

    infer_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    post_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
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
    last_error: str = ""
    last_power_error: str = ""
    last_rx_time: float = 0.0
    firmware_connected: bool = False
    power_connected: bool = False
    power_in_inference: bool = False
    power_infer_avg_mw: float = 0.0
    power_idle_avg_mw: float = 0.0
    power_infer_energy_uj: float = 0.0
    power_infer_duration_ms: float = 0.0
    power_infer_hist_mw: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    power_infer_time_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    power_idle_hist_mw: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    power_idle_time_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))


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
                print(f"[firmware] Protobuf parse error: {exc}", file=sys.stderr)
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
    """Length-prefixed nanopb PowerSample frames over serial."""

    MAX_FRAME_LEN = 64
    HANDSHAKE_REQUEST = b"PM_PING\n"
    HANDSHAKE_ACK_PREFIX = "PM_ACK"

    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = serial.Serial(port, baud, timeout=timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._handshake(timeout_s)

    def close(self) -> None:
        self._ser.close()

    def _read_exact(self, n: int) -> bytes:
        data = b""
        while len(data) < n:
            chunk = self._ser.read(n - len(data))
            if not chunk:
                raise IOError("Power serial read timeout")
            data += chunk
        return data

    def _handshake(self, timeout_s: float) -> None:
        """Verify monitor responds before binary frame decoding begins."""
        deadline = time.monotonic() + max(0.5, timeout_s)
        self._ser.write(self.HANDSHAKE_REQUEST)
        self._ser.flush()
        while time.monotonic() < deadline:
            line = self._ser.readline()
            if not line:
                continue
            text = line.decode("utf-8", errors="ignore").strip()
            if not text:
                continue
            if text.startswith(self.HANDSHAKE_ACK_PREFIX):
                print(f"[power] ACK: {text}")
                self._ser.reset_input_buffer()
                return
        raise IOError("Power monitor handshake timeout (missing PM_ACK)")

    def recv_power_sample(self) -> power_sample_pb2.PowerSample:
        """Returns a parsed PowerSample protobuf message."""
        while True:
            prefix = self._read_exact(4)
            length = struct.unpack("<I", prefix)[0]
            if length == 0 or length > self.MAX_FRAME_LEN:
                # Likely misaligned — skip one byte and retry.
                self._ser.read(1)
                continue
            payload = self._read_exact(length)
            sample = power_sample_pb2.PowerSample()
            try:
                sample.ParseFromString(payload)
            except Exception as exc:
                print(f"[power] Protobuf parse error: {exc}", file=sys.stderr)
                continue
            return sample


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
    pending_display_cmds: dict[int, bool] = {}
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
                    pending_display_cmds[msg.command_id] = bool(value)
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
                    period_us = float(result.timing.nn_period_us)
                    fps = (1000000.0 / period_us) if period_us > 0.0 else 0.0
                    now = time.time()
                    state.infer_hist.append(float(result.timing.inference_us))
                    state.post_hist.append(float(result.timing.postprocess_us))
                    state.period_hist.append(period_us)
                    state.fps_hist.append(fps)
                    state.timing_time_hist.append(now)
                    state.frame_drops = int(result.timing.frame_drops)

                if result.HasField("cpu"):
                    state.cpu_usage_percent = float(result.cpu.usage_percent)
                    state.cpu_hist.append(state.cpu_usage_percent)
                    state.cpu_time_hist.append(time.time())

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
                if ack.command_id in pending_display_cmds:
                    if ack.success:
                        state.display_enabled = pending_display_cmds[ack.command_id]
                    del pending_display_cmds[ack.command_id]

        except Exception as exc:  # Broad catch keeps GUI alive while cable reconnects.
            state.last_error = str(exc)
            state.firmware_connected = False
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

            sample = power_link.recv_power_sample()
            avg_mw = float(sample.avg_mw)
            duration_us = float(sample.duration_us)
            is_inference = bool(sample.is_inference)

            state.power_in_inference = is_inference
            now = time.time()

            if is_inference:
                state.power_infer_hist_mw.append(avg_mw)
                state.power_infer_time_hist.append(now)
                state.power_infer_avg_mw = avg_mw
                state.power_infer_duration_ms = duration_us / 1000.0
                state.power_infer_energy_uj = avg_mw * duration_us / 1000.0
            else:
                state.power_idle_hist_mw.append(avg_mw)
                state.power_idle_time_hist.append(now)
                state.power_idle_avg_mw = avg_mw
        except Exception as exc:  # Keep GUI alive if power monitor disconnects.
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


def _style_axis(ax, bg: str) -> None:
    """Apply consistent professional styling to an axes."""
    ax.set_facecolor(bg)
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)
    for spine in ("bottom", "left"):
        ax.spines[spine].set_color("#3a3a5c")
    ax.tick_params(colors="#b0b0c8", labelsize=9)
    ax.grid(True, linestyle=":", alpha=0.15, color="white")


def _legend(ax, bg: str) -> None:
    """Add a clean legend to an axes."""
    leg = ax.legend(loc="upper left", fontsize=9, framealpha=0.85, edgecolor="none", facecolor=bg)
    for text in leg.get_texts():
        text.set_color("#d0d0e0")


def create_gui(
    state: VisualizerState,
    cmd_queue: Queue[tuple[str, Optional[bool]]],
) -> tuple[list[plt.Figure], list[FuncAnimation]]:
    # --- Theme constants ---
    C_FIG_BG = "#000000"
    C_AX_BG = "#0a0a14"
    C_TEXT = "#d8d8e8"
    C_TITLE = "#e8e8f0"
    C_ACCENT = "#ED7D31"
    C_INFER = "#5B9BD5"
    C_PERIOD = "#ED7D31"
    C_POWER_INF = "#E06666"
    C_POWER_IDLE = "#5B9BD5"
    C_CPU = "#6AA84F"
    LW = 2.2

    plt.style.use("dark_background")
    plt.rcParams.update({
        "font.size": 10,
        "axes.titlesize": 13,
        "axes.titleweight": "semibold",
        "axes.labelsize": 10,
        "figure.facecolor": C_FIG_BG,
        "axes.facecolor": C_AX_BG,
        "text.color": C_TEXT,
        "axes.labelcolor": C_TEXT,
        "xtick.color": "#b0b0c8",
        "ytick.color": "#b0b0c8",
    })

    # --- Timing figure ---
    fig_timing = plt.figure("Performance Timing", figsize=(8, 5))
    fig_timing.set_facecolor(C_FIG_BG)
    ax_timing = fig_timing.add_subplot(111)

    # --- CPU figure ---
    fig_cpu = plt.figure("CPU Utilisation", figsize=(6, 5))
    fig_cpu.set_facecolor(C_FIG_BG)
    ax_cpu = fig_cpu.add_subplot(111)

    # --- ToF figure ---
    fig_tof = plt.figure("ToF Depth Grid", figsize=(6, 5))
    fig_tof.set_facecolor(C_FIG_BG)
    ax_tof = fig_tof.add_subplot(111)

    # --- Power figure ---
    fig_power = plt.figure("Power Consumption", figsize=(8, 5))
    fig_power.set_facecolor(C_FIG_BG)
    ax_power = fig_power.add_subplot(111)

    # --- Info figure ---
    fig_info = plt.figure("Device Info", figsize=(8, 7))
    fig_info.set_facecolor(C_FIG_BG)
    grid_info = fig_info.add_gridspec(2, 1, height_ratios=[9, 0.5])
    ax_text = fig_info.add_subplot(grid_info[0])
    ax_text.axis("off")
    ax_text.set_facecolor(C_FIG_BG)

    # --- Buttons ---
    ax_btn_info = fig_info.add_subplot(grid_info[1])
    ax_btn_info.axis("off")
    btn_ax1 = plt.axes([0.25, 0.02, 0.2, 0.04])
    btn_ax2 = plt.axes([0.55, 0.02, 0.2, 0.04])
    btn_info = Button(btn_ax1, "Get Device Info", color="#252545", hovercolor="#35355a")
    btn_toggle = Button(btn_ax2, "Toggle Display", color="#252545", hovercolor="#35355a")
    for btn in (btn_info, btn_toggle):
        btn.label.set_color(C_TEXT)
        btn.label.set_fontsize(10)

    # --- Timing plot ---
    _style_axis(ax_timing, C_AX_BG)
    (line_inf,) = ax_timing.plot(
        [], [], label="Inference", color=C_INFER, linewidth=LW
    )
    (line_period,) = ax_timing.plot(
        [], [], label="NN Period", color=C_PERIOD, linewidth=LW
    )
    ax_timing.set_title("Performance Timing", color=C_TITLE, pad=10)
    ax_timing.set_xlabel("Time (s)")
    ax_timing.set_ylabel("Microseconds")
    _legend(ax_timing, C_AX_BG)

    # --- ToF heatmap ---
    ax_tof.set_facecolor(C_AX_BG)
    img = ax_tof.imshow(
        np.full((8, 8), np.nan),
        cmap="magma",
        interpolation="nearest",
        vmin=0,
        vmax=2000,
    )
    cbar = fig_tof.colorbar(img, ax=ax_tof, fraction=0.046, pad=0.04)
    cbar.set_label("Distance (mm)", rotation=270, labelpad=15, color=C_TEXT)
    cbar.ax.yaxis.set_tick_params(color="#b0b0c8")
    for label in cbar.ax.get_yticklabels():
        label.set_color("#b0b0c8")
    ax_tof.set_title("ToF Depth Grid (8x8)", color=C_TITLE, pad=10)
    ax_tof.set_xticks(np.arange(-0.5, 8, 1), minor=True)
    ax_tof.set_yticks(np.arange(-0.5, 8, 1), minor=True)
    ax_tof.grid(which="minor", color="w", linestyle="-", linewidth=0.5, alpha=0.15)
    ax_tof.tick_params(which="minor", bottom=False, left=False)
    ax_tof.set_xticks([])
    ax_tof.set_yticks([])

    # --- Power plot ---
    _style_axis(ax_power, C_AX_BG)
    (line_power_inf,) = ax_power.plot(
        [], [], label="Inference", color=C_POWER_INF, linewidth=LW
    )
    (line_power_idle,) = ax_power.plot(
        [], [], label="Idle", color=C_POWER_IDLE, linewidth=LW
    )
    ax_power.set_title("Power Consumption", color=C_TITLE, pad=10)
    ax_power.set_xlabel("Time (s)")
    ax_power.set_ylabel("mW")
    _legend(ax_power, C_AX_BG)
    power_peak_text = ax_power.text(
        0.98,
        0.95,
        "",
        transform=ax_power.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color=C_ACCENT,
    )

    # --- CPU line chart ---
    _style_axis(ax_cpu, C_AX_BG)
    (line_cpu,) = ax_cpu.plot(
        [], [], label="CPU %", color=C_CPU, linewidth=LW
    )
    ax_cpu.set_ylim(auto=True)
    ax_cpu.set_title("CPU Utilisation", color=C_TITLE, pad=10)
    ax_cpu.set_xlabel("Time (s)")
    ax_cpu.set_ylabel("%")
    cpu_percent_text = ax_cpu.text(
        0.98,
        0.95,
        "0%",
        transform=ax_cpu.transAxes,
        ha="right",
        va="top",
        fontsize=15,
        fontweight="bold",
        color=C_TEXT,
    )
    cpu_freq_text = ax_cpu.text(
        0.98,
        0.05,
        "",
        transform=ax_cpu.transAxes,
        ha="right",
        va="bottom",
        fontsize=9,
        color="#9898b8",
    )

    # --- Text box ---
    text_box = ax_text.text(
        0.0,
        1.0,
        "",
        va="top",
        ha="left",
        family="monospace",
        fontsize=8.5,
        color=C_TEXT,
        linespacing=1.35,
    )

    fig_timing.tight_layout()
    fig_cpu.tight_layout()
    fig_tof.tight_layout()
    fig_power.tight_layout()
    fig_info.tight_layout()

    def on_get_info(_event) -> None:
        cmd_queue.put(("get_info", None))

    def on_toggle_display(_event) -> None:
        cmd_queue.put(("set_display", not state.display_enabled))

    btn_info.on_clicked(on_get_info)
    btn_toggle.on_clicked(on_toggle_display)
    fig_info._visualizer_widgets = (btn_info, btn_toggle)

    SEP = "\u2500" * 30

    def update_timing(_frame_idx):
        if state.timing_time_hist:
            t0 = state.timing_time_hist[0]
            x = np.array([t - t0 for t in state.timing_time_hist])
        else:
            x = np.array([])
        line_inf.set_data(x, np.array(state.infer_hist))
        line_period.set_data(x, np.array(state.period_hist))
        ax_timing.relim()
        ax_timing.autoscale_view()
        return (line_inf, line_period)

    def update_cpu(_frame_idx):
        pct = state.cpu_usage_percent
        mcu_mhz = int(state.mcu_freq_mhz)
        cpu_usage_mhz = (pct / 100.0) * float(mcu_mhz) if mcu_mhz > 0 else 0.0
        if state.cpu_time_hist:
            t0_cpu = state.cpu_time_hist[0]
            x_cpu = np.array([t - t0_cpu for t in state.cpu_time_hist])
        else:
            x_cpu = np.array([])
        line_cpu.set_data(x_cpu, np.array(state.cpu_hist))
        ax_cpu.relim()
        ax_cpu.autoscale_view()
        cpu_percent_text.set_text(f"{pct:.1f}%")
        npu = int(state.npu_freq_mhz)
        if mcu_mhz > 0:
            cpu_freq_text.set_text(f"{cpu_usage_mhz:.0f} MHz  |  CPU {mcu_mhz} MHz / NPU {npu} MHz")
        else:
            cpu_freq_text.set_text(f"CPU {mcu_mhz} MHz / NPU {npu} MHz")
        return (line_cpu, cpu_percent_text, cpu_freq_text)

    def update_tof(_frame_idx):
        img.set_data(state.tof_grid)
        return (img,)

    def update_power(_frame_idx):
        t0_power = min(
            state.power_infer_time_hist[0] if state.power_infer_time_hist else float("inf"),
            state.power_idle_time_hist[0] if state.power_idle_time_hist else float("inf"),
        )
        if state.power_infer_time_hist:
            x_inf = np.array([t - t0_power for t in state.power_infer_time_hist])
        else:
            x_inf = np.array([])
        if state.power_idle_time_hist:
            x_idle = np.array([t - t0_power for t in state.power_idle_time_hist])
        else:
            x_idle = np.array([])
        line_power_inf.set_data(x_inf, np.array(state.power_infer_hist_mw))
        line_power_idle.set_data(x_idle, np.array(state.power_idle_hist_mw))
        power_peak_text.set_text(f"infer: {state.power_infer_avg_mw:.0f} mW   idle: {state.power_idle_avg_mw:.0f} mW")
        ax_power.relim()
        ax_power.autoscale_view()
        return (line_power_inf, line_power_idle, power_peak_text)

    def update_info(_frame_idx):
        status = "ALERT" if state.tof_alert else "OK"
        stale = "stale" if state.tof_stale else "fresh"
        class_labels = ", ".join(state.class_labels) or "-"
        recog_host_fw = "yes" if state.host_introduced else "no"
        recog_fw_host = "yes" if state.firmware_recognized else "no"
        detections_text = state.detections_text or "  -"
        last_pp = state.post_hist[-1] if state.post_hist else 0.0
        npu_delta = (
            f"{int(state.power_infer_avg_mw - state.power_idle_avg_mw)} mW"
            if state.power_idle_avg_mw > 0
            else "-"
        )

        lines = [
            f" Device",
            SEP,
            f"  Model   {state.model_name}   NN: {state.nn_size_text}",
            f"  Display {state.display_width}x{state.display_height}   Letterbox: {state.letterbox_width}x{state.letterbox_height}",
            f"  Camera  {state.camera_mode_text}   Mode: {state.build_mode_text}",
            f"  Built   {state.build_timestamp}",
            "",
            f" Connection",
            SEP,
            f"  STM32   {'connected' if state.firmware_connected else 'disconnected'}   H\u2192D: {recog_host_fw}  D\u2192H: {recog_fw_host}",
            f"  Display {'on' if state.display_enabled else 'off'}",
            "",
            f" Sensors",
            SEP,
            f"  ToF     hand {state.hand_mm} mm   haz {state.hazard_mm} mm   {status} ({stale})",
            f"  Power   infer {state.power_infer_avg_mw:.0f} mW ({state.power_infer_duration_ms:.1f} ms)   idle {state.power_idle_avg_mw:.0f} mW",
            f"          energy {state.power_infer_energy_uj:.0f} uJ   npu delta {npu_delta}",
            f"  ESP32   {'connected' if state.power_connected else 'disconnected'}",
            "",
            f" Stats",
            SEP,
            f"  Frames  {state.frame_count}   Drops: {state.frame_drops}   PP: {last_pp:.0f} us",
            f"  Labels  {class_labels}",
            f"  ACK     {state.last_ack}",
            f"  Err     {state.last_error or '-'}",
            f"  PwrErr  {state.last_power_error or '-'}",
            "",
            f" Detections ({state.detection_count})",
            SEP,
            f"  {detections_text}",
        ]
        text_box.set_text("\n".join(lines))
        return (text_box,)

    anim_timing = FuncAnimation(fig_timing, update_timing, interval=120, blit=False, cache_frame_data=False)
    anim_cpu = FuncAnimation(fig_cpu, update_cpu, interval=120, blit=False, cache_frame_data=False)
    anim_tof = FuncAnimation(fig_tof, update_tof, interval=120, blit=False, cache_frame_data=False)
    anim_power = FuncAnimation(fig_power, update_power, interval=120, blit=False, cache_frame_data=False)
    anim_info = FuncAnimation(fig_info, update_info, interval=120, blit=False, cache_frame_data=False)

    figures = [fig_timing, fig_cpu, fig_tof, fig_power, fig_info]
    animations = [anim_timing, anim_cpu, anim_tof, anim_power, anim_info]
    return figures, animations


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


def _real_ports() -> list:
    return [p for p in list_ports.comports() if p.vid is not None]


def _score_port(
    info,
    keyword_scores: dict[str, int],
    preferred_vids: set[int] | None = None,
    rejected_vids: set[int] | None = None,
) -> int:
    """Score a serial port by VID and keyword matches in its metadata."""
    if rejected_vids and info.vid in rejected_vids:
        return -1
    score = 0
    if preferred_vids and info.vid in preferred_vids:
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


def _rank_ports(ports: list, scorer) -> list[tuple[object, int]]:
    """Return ports sorted by (score, device) descending, with cached scores."""
    scored = [(p, scorer(p)) for p in ports]
    scored.sort(key=lambda t: (t[1], t[0].device), reverse=True)
    return scored


def resolve_port(port: Optional[str]) -> str:
    if port:
        return port

    ports = _real_ports()
    if not ports:
        raise RuntimeError("No serial ports found. Pass port explicitly.")

    _ESPRESSIF_VIDS = {0x303A}
    _ST_VIDS = {0x0483}
    _FW_KEYWORDS = {
        "stm32": 6, "stmicro": 6, "stlink": 5,
        "nucleo": 4, "discovery": 4, "virtual com": 3, "vcp": 3, "usb serial": 1,
    }
    scorer = lambda info: _score_port(info, _FW_KEYWORDS, preferred_vids=_ST_VIDS, rejected_vids=_ESPRESSIF_VIDS)
    ranked = _rank_ports(ports, scorer)

    selected, selected_score = ranked[0]
    if selected_score <= 0 and len(ranked) > 1:
        candidates = ", ".join(p.device for p, _ in ranked[:4])
        print(
            f"[visualizer] Could not confidently identify firmware port; choosing {selected.device} from: {candidates}"
        )
    else:
        print(
            f"[visualizer] Auto-selected firmware-like port: {selected.device} (score={selected_score})"
        )
    return selected.device


def resolve_power_port(port: Optional[str]) -> Optional[str]:
    if port:
        return port

    ports = _real_ports()
    if not ports:
        return None

    _ESPRESSIF_VIDS = {0x303A}
    _POWER_KEYWORDS = {"esp32c6": 8, "c6": 7, "esp32": 6, "espressif": 6, "esp": 4, "jtag": 2}
    scorer = lambda info: _score_port(info, _POWER_KEYWORDS, preferred_vids=_ESPRESSIF_VIDS)
    ranked = _rank_ports(ports, scorer)

    best, best_score = ranked[0]
    if best_score <= 0:
        return None
    print(
        f"[visualizer] Auto-selected power monitor port: {best.device} (score={best_score})"
    )
    return best.device


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
    power_port = resolve_power_port(args.power_port)
    power_thread = threading.Thread(
        target=power_receiver_loop,
        args=(power_port, args.power_baud, args.timeout, state, stop_evt),
        daemon=True,
    )
    power_thread.start()

    # Handshake: host introduces itself by requesting device metadata.
    cmd_queue.put(("get_info", None))

    figures, animations = create_gui(state, cmd_queue)
    try:
        plt.show()
    finally:
        stop_evt.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
