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
    power_sync: bool = False
    power_infer_avg_mw: float = 0.0
    power_idle_avg_mw: float = 0.0
    power_infer_energy_uj: float = 0.0
    power_infer_duration_ms: float = 0.0
    power_hist_mw: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    sync_hist: deque[bool] = field(default_factory=lambda: deque(maxlen=240))


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
    """Length-prefixed nanopb PowerSample frames over serial."""

    MAX_FRAME_LEN = 64

    def __init__(self, port: str, baud: int, timeout_s: float):
        self._ser = serial.Serial(port, baud, timeout=timeout_s)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        self._ser.write(b"START\n")
        self._ser.flush()
        # Drain text preamble lines (# comments) before protobuf stream.
        self._ser.timeout = 2.0
        while True:
            line = self._ser.readline()
            if not line:
                break
            text = line.decode("utf-8", errors="ignore").strip()
            if text.startswith("# Streaming"):
                break
        self._ser.timeout = timeout_s

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
            except Exception:
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
                if ack.command_id in pending_display_cmds:
                    if ack.success:
                        state.display_enabled = pending_display_cmds[ack.command_id]
                    del pending_display_cmds[ack.command_id]

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

            # Append to history for the power plot.
            state.power_hist_mw.append(avg_mw)
            state.sync_hist.append(is_inference)
            state.power_sync = is_inference

            if is_inference:
                state.power_infer_avg_mw = avg_mw
                state.power_infer_duration_ms = duration_us / 1000.0
                state.power_infer_energy_uj = avg_mw * duration_us / 1000.0
            else:
                state.power_idle_avg_mw = avg_mw
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
    btn_info = Button(
        ax_btn_info, "Get Device Info", color="#333333", hovercolor="#555555"
    )
    btn_toggle = Button(
        ax_btn_toggle, "Toggle Display", color="#333333", hovercolor="#555555"
    )
    btn_info.label.set_color("white")
    btn_toggle.label.set_color("white")

    # --- Timing plot ---
    (line_inf,) = ax_timing.plot(
        [], [], label="Inference", color="#00ff00", linewidth=2
    )
    (line_period,) = ax_timing.plot(
        [], [], label="NN Period", color="#ffaa00", linewidth=2
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

    sync_fill_ref = [None]

    def on_get_info(_event) -> None:
        cmd_queue.put(("get_info", None))

    def on_toggle_display(_event) -> None:
        cmd_queue.put(("set_display", not state.display_enabled))

    btn_info.on_clicked(on_get_info)
    btn_toggle.on_clicked(on_toggle_display)
    fig._visualizer_widgets = (btn_info, btn_toggle)

    def update(_frame_idx):
        # --- Timing ---
        x = np.arange(len(state.infer_hist))
        line_inf.set_data(x, np.array(state.infer_hist))
        line_period.set_data(x, np.array(state.period_hist))
        ax_timing.relim()
        ax_timing.autoscale_view()

        # --- ToF ---
        img.set_data(state.tof_grid)

        # --- Power ---
        x_power = np.arange(len(state.power_hist_mw))
        line_power.set_data(x_power, np.array(state.power_hist_mw))

        power_peak_text.set_text(
            f"infer: {state.power_infer_avg_mw:.0f}mW  idle: {state.power_idle_avg_mw:.0f}mW"
        )

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
            f"Disp: {'on' if state.display_enabled else 'off'}",
            f"ToF: hand={state.hand_mm}mm haz={state.hazard_mm}mm {status}({stale})",
            f"ESP32: {'yes' if state.power_connected else 'no'}  infer={state.power_infer_avg_mw:.0f}mW ({state.power_infer_duration_ms:.1f}ms)  idle={state.power_idle_avg_mw:.0f}mW",
            f"  energy={state.power_infer_energy_uj:.0f}uJ  npu_delta={'%dmW' % int(state.power_infer_avg_mw - state.power_idle_avg_mw) if state.power_idle_avg_mw > 0 else '-'}",
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
            img,
            text_box,
            line_power,
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


def resolve_port(port: Optional[str]) -> str:
    if port:
        return port

    ports = _real_ports()
    if not ports:
        raise RuntimeError("No serial ports found. Pass port explicitly.")

    espressif_vids = {0x303A}
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
        if info.vid in espressif_vids:
            return -1
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
    if selected_score <= 0 and len(ranked) > 1:
        candidates = ", ".join(p.device for p in ranked[:4])
        print(
            f"[visualizer] Could not confidently identify firmware port; choosing {selected} from: {candidates}"
        )
    else:
        print(
            f"[visualizer] Auto-selected firmware-like port: {selected} (score={selected_score})"
        )
    return selected


def resolve_power_port(port: Optional[str]) -> Optional[str]:
    if port:
        return port

    espressif_vids = {0x303A}
    keyword_scores = {
        "esp32": 6,
        "espressif": 6,
        "esp": 4,
        "jtag": 2,
    }

    ports = _real_ports()
    if not ports:
        return None

    def score_port(info) -> int:
        score = 0
        if info.vid in espressif_vids:
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
    best = ranked[0]
    best_score = score_port(best)
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

    fig, _anim = create_gui(state, cmd_queue)
    try:
        plt.show()
    finally:
        stop_evt.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
