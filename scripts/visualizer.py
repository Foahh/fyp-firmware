#!/usr/bin/env python3
"""Serial visualizer for Appli/Serial protobuf stream."""

from __future__ import annotations

import argparse
import csv
import os
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from datetime import datetime
from queue import Empty, Queue
from typing import Any, Optional

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
    pp_conf_threshold: float = 0.1
    pp_iou_threshold: float = 0.45
    track_thresh: float = 0.25
    det_thresh: float = 0.8
    sim1_thresh: float = 0.8
    sim2_thresh: float = 0.5
    tlost_cnt: int = 30
    # Multi-line status for Runtime PP config window; GUI sets "Sent…"; RX appends ACK/DeviceInfo.
    pp_cfg_status_text: str = ""
    pp_cfg_pending_device_info: int = 0

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

    person_mm: list[int] = field(default_factory=list)
    tof_alert: bool = False
    tof_stale: bool = True
    tof_depth_grid: np.ndarray = field(
        default_factory=lambda: np.full((8, 8), np.nan, dtype=np.float32)
    )
    tof_sigma_grid: np.ndarray = field(
        default_factory=lambda: np.full((8, 8), np.nan, dtype=np.float32)
    )
    tof_signal_grid: np.ndarray = field(
        default_factory=lambda: np.full((8, 8), np.nan, dtype=np.float32)
    )

    display_enabled: bool = True
    last_ack: str = "No ACK received."
    last_ack_timestamp: int = 0
    last_error: str = ""
    last_power_error: str = ""
    last_rx_time: float = 0.0
    # Wall-clock origin for GUI time axes (set on first sample in any stream).
    plot_epoch_s: Optional[float] = None
    firmware_connected: bool = False
    firmware_port_path: str = ""
    firmware_baud: int = 0
    power_connected: bool = False
    power_port_path: str = ""
    power_baud: int = 0
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

    # Per-phase energy from power monitor (mJ); period total = last inf + last idle
    pm_avg_inf_mj_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    pm_avg_inf_mj_time_hist: deque[float] = field(
        default_factory=lambda: deque(maxlen=240)
    )
    pm_avg_idle_mj_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    pm_avg_idle_mj_time_hist: deque[float] = field(
        default_factory=lambda: deque(maxlen=240)
    )
    pm_last_inf_mj: float = 0.0
    pm_last_idle_mj: float = 0.0
    pm_last_inf_duration_us: float = 0.0
    pm_last_idle_duration_us: float = 0.0
    pm_period_total_mj: float = 0.0
    pm_seen_infer_mj: bool = False
    pm_seen_idle_mj: bool = False
    pm_period_mj_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
    pm_period_mj_time_hist: deque[float] = field(
        default_factory=lambda: deque(maxlen=240)
    )

    # Battery estimate: capacity in mAh; energy (mWh) = mAh × supply V; runtime h = mWh / P_avg mW.
    # P_avg uses period mJ / (last infer + last idle duration from power monitor), not nn_period_us.
    battery_capacity_mah: float = 820.0
    battery_supply_voltage_v: float = 5.0  # STM32 input rail for mWh = mAh × V
    battery_p_avg_mw_hist: deque[float] = field(
        default_factory=lambda: deque(maxlen=240)
    )
    battery_time_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))


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


def _touch_plot_epoch(state: VisualizerState, now: float) -> None:
    if state.plot_epoch_s is None:
        state.plot_epoch_s = now


def reshape_tof_grid(values: Any) -> np.ndarray:
    if len(values) != 64:
        return np.full((8, 8), np.nan, dtype=np.float32)
    return np.array(values, dtype=np.float32).reshape(8, 8)


def _iso_timestamp(timestamp_s: float) -> str:
    return datetime.fromtimestamp(timestamp_s).astimezone().isoformat(timespec="seconds")


def _flatten_sequence(prefix: str, values: list[Any], width: int) -> dict[str, Any]:
    row = {}
    padded = list(values[:width]) + [""] * max(0, width - len(values))
    for idx, value in enumerate(padded):
        row[f"{prefix}_{idx:02d}"] = value
    return row


class RecordingManager:
    def __init__(self, output_root: str) -> None:
        self.output_root = output_root
        self._lock = threading.Lock()
        self._active = False
        self._session_name = ""
        self._session_dir = ""
        self._start_time_s = 0.0
        self._stop_time_s = 0.0
        self._detection_rows: list[dict[str, Any]] = []
        self._detection_item_rows: list[dict[str, Any]] = []
        self._track_rows: list[dict[str, Any]] = []
        self._device_info_rows: list[dict[str, Any]] = []
        self._ack_rows: list[dict[str, Any]] = []
        self._tof_rows: list[dict[str, Any]] = []
        self._tof_alert_rows: list[dict[str, Any]] = []
        self._cpu_rows: list[dict[str, Any]] = []
        self._power_rows: list[dict[str, Any]] = []

    def _reset_rows_locked(self) -> None:
        self._detection_rows = []
        self._detection_item_rows = []
        self._track_rows = []
        self._device_info_rows = []
        self._ack_rows = []
        self._tof_rows = []
        self._tof_alert_rows = []
        self._cpu_rows = []
        self._power_rows = []

    def _base_row_locked(self, host_time_s: float) -> dict[str, Any]:
        return {
            "host_time_s": f"{host_time_s:.6f}",
            "record_elapsed_s": f"{host_time_s - self._start_time_s:.6f}",
            "recorded_at_iso": _iso_timestamp(host_time_s),
        }

    def _device_info_row_from_state(self, state: VisualizerState) -> dict[str, Any]:
        host_time_s = time.time()
        return {
            **self._base_row_locked(host_time_s),
            "timestamp_ms": int(state.device_info_timestamp),
            "display_width_px": int(state.display_width),
            "display_height_px": int(state.display_height),
            "letterbox_width_px": int(state.letterbox_width),
            "letterbox_height_px": int(state.letterbox_height),
            "nn_width_px": int(state.nn_width),
            "nn_height_px": int(state.nn_height),
            "nn_input_size_bytes": int(state.nn_input_size_bytes),
            "power_mode": int(state.power_mode),
            "camera_fps": int(state.camera_fps),
            "mcu_freq_mhz": int(state.mcu_freq_mhz),
            "npu_freq_mhz": int(state.npu_freq_mhz),
            "model_name": state.model_name,
            "class_labels": "|".join(state.class_labels),
            "build_timestamp": state.build_timestamp,
            "pp_conf_threshold": f"{state.pp_conf_threshold:.6f}",
            "pp_iou_threshold": f"{state.pp_iou_threshold:.6f}",
            "track_thresh": f"{state.track_thresh:.6f}",
            "det_thresh": f"{state.det_thresh:.6f}",
            "sim1_thresh": f"{state.sim1_thresh:.6f}",
            "sim2_thresh": f"{state.sim2_thresh:.6f}",
            "tlost_cnt": int(state.tlost_cnt),
        }

    def start(self, state: Optional[VisualizerState] = None) -> str:
        with self._lock:
            if self._active:
                raise RuntimeError("Recording is already active.")
            self._reset_rows_locked()
            self._active = True
            self._start_time_s = time.time()
            self._stop_time_s = 0.0
            self._session_name = datetime.now().strftime("%Y%m%d_%H%M%S")
            self._session_dir = os.path.join(self.output_root, self._session_name)
            if state is not None and state.firmware_recognized:
                self._device_info_rows.append(self._device_info_row_from_state(state))
            return self._session_dir

    def is_recording(self) -> bool:
        with self._lock:
            return self._active

    def stop(self, state: VisualizerState) -> str:
        with self._lock:
            if not self._active:
                raise RuntimeError("Recording is not active.")
            self._active = False
            self._stop_time_s = time.time()
            session_dir = self._session_dir
            snapshot = {
                "session_name": self._session_name,
                "start_time_s": self._start_time_s,
                "stop_time_s": self._stop_time_s,
                "detection_rows": list(self._detection_rows),
                "detection_item_rows": list(self._detection_item_rows),
                "track_rows": list(self._track_rows),
                "device_info_rows": list(self._device_info_rows),
                "ack_rows": list(self._ack_rows),
                "tof_rows": list(self._tof_rows),
                "tof_alert_rows": list(self._tof_alert_rows),
                "cpu_rows": list(self._cpu_rows),
                "power_rows": list(self._power_rows),
            }
            if not snapshot["device_info_rows"] and state.firmware_recognized:
                snapshot["device_info_rows"].append(self._device_info_row_from_state(state))

        os.makedirs(session_dir, exist_ok=True)
        self._write_csvs(session_dir, snapshot)
        self._write_metadata(session_dir, snapshot, state)
        return session_dir

    def record_detection_result(
        self,
        result: messages_pb2.DetectionResult,
        class_labels: list[str],
        cpu_usage_percent: float,
    ) -> None:
        host_time_s = time.time()
        with self._lock:
            if not self._active:
                return
            sample_index = len(self._detection_rows)
            fps = (1_000_000.0 / float(result.nn_period_us)) if result.nn_period_us else 0.0
            base = self._base_row_locked(host_time_s)
            self._detection_rows.append(
                {
                    **base,
                    "sample_index": sample_index,
                    "sent_timestamp_ms": int(result.sent_timestamp_ms),
                    "frame_timestamp_ms": int(result.frame_timestamp_ms),
                    "inference_us": int(result.inference_us),
                    "postprocess_us": int(result.postprocess_us),
                    "tracker_us": int(result.tracker_us),
                    "nn_period_us": int(result.nn_period_us),
                    "fps": f"{fps:.6f}",
                    "frame_drop_count": int(result.frame_drop_count),
                    "cpu_usage_percent": f"{cpu_usage_percent:.6f}",
                    "detection_count": len(result.detections),
                    "tracked_box_count": len(result.tracks),
                }
            )
            for det_index, det in enumerate(result.detections):
                class_id = int(det.class_id)
                class_label = (
                    class_labels[class_id]
                    if 0 <= class_id < len(class_labels)
                    else str(class_id)
                )
                self._detection_item_rows.append(
                    {
                        **base,
                        "sample_index": sample_index,
                        "frame_timestamp_ms": int(result.frame_timestamp_ms),
                        "detection_index": det_index,
                        "class_id": class_id,
                        "class_label": class_label,
                        "score": f"{float(det.score):.6f}",
                        "x": f"{float(det.x):.6f}",
                        "y": f"{float(det.y):.6f}",
                        "w": f"{float(det.w):.6f}",
                        "h": f"{float(det.h):.6f}",
                    }
                )
            for track_index, track in enumerate(result.tracks):
                self._track_rows.append(
                    {
                        **base,
                        "sample_index": sample_index,
                        "frame_timestamp_ms": int(result.frame_timestamp_ms),
                        "track_index": track_index,
                        "track_id": int(track.track_id),
                        "x": f"{float(track.x):.6f}",
                        "y": f"{float(track.y):.6f}",
                        "w": f"{float(track.w):.6f}",
                        "h": f"{float(track.h):.6f}",
                    }
                )

    def record_device_info(self, info: messages_pb2.DeviceInfo) -> None:
        host_time_s = time.time()
        with self._lock:
            if not self._active:
                return
            self._device_info_rows.append(
                {
                    **self._base_row_locked(host_time_s),
                    "timestamp_ms": int(info.timestamp_ms),
                    "display_width_px": int(info.display_width_px),
                    "display_height_px": int(info.display_height_px),
                    "letterbox_width_px": int(info.letterbox_width_px),
                    "letterbox_height_px": int(info.letterbox_height_px),
                    "nn_width_px": int(info.nn_width_px),
                    "nn_height_px": int(info.nn_height_px),
                    "nn_input_size_bytes": int(info.nn_input_size_bytes),
                    "power_mode": int(info.power_mode),
                    "camera_fps": int(info.camera_fps),
                    "mcu_freq_mhz": int(info.mcu_freq_mhz),
                    "npu_freq_mhz": int(info.npu_freq_mhz),
                    "model_name": info.model_name,
                    "class_labels": "|".join(info.class_labels),
                    "build_timestamp": info.build_timestamp,
                    "pp_conf_threshold": f"{float(info.pp_conf_threshold):.6f}",
                    "pp_iou_threshold": f"{float(info.pp_iou_threshold):.6f}",
                    "track_thresh": f"{float(info.track_thresh):.6f}",
                    "det_thresh": f"{float(info.det_thresh):.6f}",
                    "sim1_thresh": f"{float(info.sim1_thresh):.6f}",
                    "sim2_thresh": f"{float(info.sim2_thresh):.6f}",
                    "tlost_cnt": int(info.tlost_cnt),
                }
            )

    def record_ack(self, ack: messages_pb2.Ack) -> None:
        host_time_s = time.time()
        with self._lock:
            if not self._active:
                return
            self._ack_rows.append(
                {
                    **self._base_row_locked(host_time_s),
                    "success": bool(ack.success),
                    "timestamp_ms": int(ack.timestamp_ms),
                }
            )

    def record_tof_result(self, tof_result: messages_pb2.TofResult) -> None:
        host_time_s = time.time()
        with self._lock:
            if not self._active:
                return
            row = {
                **self._base_row_locked(host_time_s),
                "timestamp_ms": int(tof_result.timestamp_ms),
                "tof_period_us": int(tof_result.tof_period_us),
            }
            row.update(_flatten_sequence("depth_mm", list(tof_result.depth_mm), 64))
            row.update(
                _flatten_sequence(
                    "range_sigma_mm", list(tof_result.range_sigma_mm), 64
                )
            )
            row.update(
                _flatten_sequence("signal_per_spad", list(tof_result.signal_per_spad), 64)
            )
            self._tof_rows.append(row)

    def record_tof_alert_result(
        self,
        tof_alert_result: messages_pb2.TofAlertResult,
    ) -> None:
        host_time_s = time.time()
        with self._lock:
            if not self._active:
                return
            row = {
                **self._base_row_locked(host_time_s),
                "timestamp_ms": int(tof_alert_result.timestamp_ms),
                "flags": int(tof_alert_result.flags),
                "alert": bool(int(tof_alert_result.flags) & TOF_PB_FLAG_ALERT),
                "stale": bool(int(tof_alert_result.flags) & TOF_PB_FLAG_STALE),
                "fusion_period_us": int(tof_alert_result.fusion_period_us),
                "person_count": len(tof_alert_result.person_mm),
            }
            row.update(_flatten_sequence("person_mm", list(tof_alert_result.person_mm), 4))
            self._tof_alert_rows.append(row)

    def record_cpu_usage_sample(self, cpu_sample: messages_pb2.CpuUsageSample) -> None:
        host_time_s = time.time()
        with self._lock:
            if not self._active:
                return
            self._cpu_rows.append(
                {
                    **self._base_row_locked(host_time_s),
                    "timestamp_ms": int(cpu_sample.timestamp_ms),
                    "cpu_usage_percent": f"{float(cpu_sample.cpu_usage_percent):.6f}",
                }
            )

    def record_power_sample(self, sample: power_sample_pb2.PowerSample) -> None:
        host_time_s = time.time()
        duration_s = float(sample.duration_us) * 1e-6
        average_mw = (
            (float(sample.energy_j) / duration_s) * 1000.0 if duration_s > 0.0 else 0.0
        )
        with self._lock:
            if not self._active:
                return
            self._power_rows.append(
                {
                    **self._base_row_locked(host_time_s),
                    "timestamp_us": int(sample.timestamp_us),
                    "energy_j": f"{float(sample.energy_j):.9f}",
                    "energy_mj": f"{float(sample.energy_j) * 1000.0:.6f}",
                    "duration_us": int(sample.duration_us),
                    "is_inference": bool(sample.is_inference),
                    "average_mw": f"{average_mw:.6f}",
                }
            )

    def _write_csvs(self, session_dir: str, snapshot: dict[str, Any]) -> None:
        self._write_csv(
            os.path.join(session_dir, "detection_result.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "sample_index",
                "sent_timestamp_ms",
                "frame_timestamp_ms",
                "inference_us",
                "postprocess_us",
                "tracker_us",
                "nn_period_us",
                "fps",
                "frame_drop_count",
                "cpu_usage_percent",
                "detection_count",
                "tracked_box_count",
            ],
            snapshot["detection_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "detections.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "sample_index",
                "frame_timestamp_ms",
                "detection_index",
                "class_id",
                "class_label",
                "score",
                "x",
                "y",
                "w",
                "h",
            ],
            snapshot["detection_item_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "tracks.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "sample_index",
                "frame_timestamp_ms",
                "track_index",
                "track_id",
                "x",
                "y",
                "w",
                "h",
            ],
            snapshot["track_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "device_info.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "timestamp_ms",
                "display_width_px",
                "display_height_px",
                "letterbox_width_px",
                "letterbox_height_px",
                "nn_width_px",
                "nn_height_px",
                "nn_input_size_bytes",
                "power_mode",
                "camera_fps",
                "mcu_freq_mhz",
                "npu_freq_mhz",
                "model_name",
                "class_labels",
                "build_timestamp",
                "pp_conf_threshold",
                "pp_iou_threshold",
                "track_thresh",
                "det_thresh",
                "sim1_thresh",
                "sim2_thresh",
                "tlost_cnt",
            ],
            snapshot["device_info_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "ack.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "success",
                "timestamp_ms",
            ],
            snapshot["ack_rows"],
        )
        tof_fieldnames = [
            "host_time_s",
            "record_elapsed_s",
            "recorded_at_iso",
            "timestamp_ms",
            "tof_period_us",
        ]
        tof_fieldnames.extend([f"depth_mm_{idx:02d}" for idx in range(64)])
        tof_fieldnames.extend([f"range_sigma_mm_{idx:02d}" for idx in range(64)])
        tof_fieldnames.extend([f"signal_per_spad_{idx:02d}" for idx in range(64)])
        self._write_csv(
            os.path.join(session_dir, "tof_result.csv"),
            tof_fieldnames,
            snapshot["tof_rows"],
        )
        tof_alert_fieldnames = [
            "host_time_s",
            "record_elapsed_s",
            "recorded_at_iso",
            "timestamp_ms",
            "flags",
            "alert",
            "stale",
            "fusion_period_us",
            "person_count",
        ]
        tof_alert_fieldnames.extend([f"person_mm_{idx:02d}" for idx in range(4)])
        self._write_csv(
            os.path.join(session_dir, "tof_alert_result.csv"),
            tof_alert_fieldnames,
            snapshot["tof_alert_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "cpu_usage.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "timestamp_ms",
                "cpu_usage_percent",
            ],
            snapshot["cpu_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "power_sample.csv"),
            [
                "host_time_s",
                "record_elapsed_s",
                "recorded_at_iso",
                "timestamp_us",
                "energy_j",
                "energy_mj",
                "duration_us",
                "is_inference",
                "average_mw",
            ],
            snapshot["power_rows"],
        )

    def _write_csv(
        self,
        path: str,
        fieldnames: list[str],
        rows: list[dict[str, Any]],
    ) -> None:
        with open(path, "w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)

    def _write_metadata(
        self,
        session_dir: str,
        snapshot: dict[str, Any],
        state: VisualizerState,
    ) -> None:
        start_time_s = float(snapshot["start_time_s"])
        stop_time_s = float(snapshot["stop_time_s"])
        duration_s = max(0.0, stop_time_s - start_time_s)
        counts = {
            "detection_result": len(snapshot["detection_rows"]),
            "detections": len(snapshot["detection_item_rows"]),
            "tracks": len(snapshot["track_rows"]),
            "device_info": len(snapshot["device_info_rows"]),
            "ack": len(snapshot["ack_rows"]),
            "tof_result": len(snapshot["tof_rows"]),
            "tof_alert_result": len(snapshot["tof_alert_rows"]),
            "cpu_usage": len(snapshot["cpu_rows"]),
            "power_sample": len(snapshot["power_rows"]),
        }
        lines = [
            f"session_name: {snapshot['session_name']}",
            f"output_dir: {session_dir}",
            f"started_at: {_iso_timestamp(start_time_s)}",
            f"stopped_at: {_iso_timestamp(stop_time_s)}",
            f"duration_s: {duration_s:.3f}",
            "",
            "[connection]",
            f"firmware_connected: {state.firmware_connected}",
            f"firmware_port_path: {state.firmware_port_path or '-'}",
            f"firmware_baud: {state.firmware_baud or '-'}",
            f"power_connected: {state.power_connected}",
            f"power_port_path: {state.power_port_path or '-'}",
            f"power_baud: {state.power_baud or '-'}",
            "",
            "[device]",
            f"model_name: {state.model_name}",
            f"build_timestamp: {state.build_timestamp}",
            f"display_enabled: {state.display_enabled}",
            f"display_size_px: {state.display_width}x{state.display_height}",
            f"letterbox_size_px: {state.letterbox_width}x{state.letterbox_height}",
            f"nn_size_px: {state.nn_width}x{state.nn_height}",
            f"nn_input_size_bytes: {state.nn_input_size_bytes}",
            f"camera_fps: {state.camera_fps}",
            f"mcu_freq_mhz: {state.mcu_freq_mhz}",
            f"npu_freq_mhz: {state.npu_freq_mhz}",
            f"class_labels: {', '.join(state.class_labels) or '-'}",
            "",
            "[runtime_config]",
            f"pp_conf_threshold: {state.pp_conf_threshold:.6f}",
            f"pp_iou_threshold: {state.pp_iou_threshold:.6f}",
            f"track_thresh: {state.track_thresh:.6f}",
            f"det_thresh: {state.det_thresh:.6f}",
            f"sim1_thresh: {state.sim1_thresh:.6f}",
            f"sim2_thresh: {state.sim2_thresh:.6f}",
            f"tlost_cnt: {state.tlost_cnt}",
            f"battery_capacity_mah: {state.battery_capacity_mah:.3f}",
            f"battery_supply_voltage_v: {state.battery_supply_voltage_v:.3f}",
            "",
            "[latest_values]",
            f"frame_count: {state.frame_count}",
            f"frame_drops: {state.frame_drops}",
            f"last_timestamp_ms: {state.last_timestamp}",
            f"detection_timestamp_ms: {state.detection_timestamp}",
            f"device_info_timestamp_ms: {state.device_info_timestamp}",
            f"last_ack_timestamp_ms: {state.last_ack_timestamp}",
            f"cpu_usage_percent: {state.cpu_usage_percent:.6f}",
            f"power_infer_avg_mw: {state.power_infer_avg_mw:.6f}",
            f"power_idle_avg_mw: {state.power_idle_avg_mw:.6f}",
            f"power_infer_energy_uj: {state.power_infer_energy_uj:.6f}",
            f"power_infer_duration_ms: {state.power_infer_duration_ms:.6f}",
            f"pm_last_inf_mj: {state.pm_last_inf_mj:.6f}",
            f"pm_last_idle_mj: {state.pm_last_idle_mj:.6f}",
            f"pm_period_total_mj: {state.pm_period_total_mj:.6f}",
            "",
            "[samples]",
        ]
        lines.extend(f"{key}: {value}" for key, value in counts.items())
        with open(
            os.path.join(session_dir, "metadata.txt"),
            "w",
            encoding="utf-8",
        ) as handle:
            handle.write("\n".join(lines) + "\n")


def receiver_loop(
    port: Optional[str],
    baud: int,
    timeout_s: float,
    state: VisualizerState,
    cmd_queue: Queue[tuple[str, Any]],
    stop_evt: threading.Event,
    recorder: RecordingManager,
) -> None:
    pending_display: deque[bool] = deque()
    pending_ack_types: deque[str] = deque()
    link: Optional[SerialLink] = None
    last_get_info_mono: float = 0.0
    GET_INFO_RESEND_S = 1.5

    def send_get_info(why: str) -> None:
        nonlocal last_get_info_mono
        assert link is not None
        msg = messages_pb2.HostMessage()
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
                pending_display.clear()
                pending_ack_types.clear()
                state.pp_cfg_pending_device_info = 0
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
                    pending_display.append(bool(value))
                    pending_ack_types.append("display")
                elif command == "set_pp_config" and isinstance(value, dict):
                    cfg = msg.set_postprocess_config
                    cfg.pp_conf_threshold = float(value["pp_conf_threshold"])
                    cfg.pp_iou_threshold = float(value["pp_iou_threshold"])
                    cfg.track_thresh = float(value["track_thresh"])
                    cfg.det_thresh = float(value["det_thresh"])
                    cfg.sim1_thresh = float(value["sim1_thresh"])
                    cfg.sim2_thresh = float(value["sim2_thresh"])
                    cfg.tlost_cnt = int(value["tlost_cnt"])
                    cfg.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
                    state.host_introduced = True
                    pending_ack_types.append("pp_config")
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
                fps = (1_000_000.0 / period_us) if period_us > 0.0 else 0.0
                now = time.time()
                _touch_plot_epoch(state, now)
                state.infer_hist.append(float(result.inference_us))
                state.post_hist.append(float(result.postprocess_us))
                state.tracker_hist.append(float(result.tracker_us))
                state.period_hist.append(period_us)
                state.fps_hist.append(fps)
                state.timing_time_hist.append(now)
                state.frame_drops = int(result.frame_drop_count)

                period_us_pm = (
                    state.pm_last_inf_duration_us + state.pm_last_idle_duration_us
                )
                if (
                    period_us_pm > 0.0
                    and state.pm_seen_infer_mj
                    and state.pm_seen_idle_mj
                    and state.pm_period_total_mj > 0.0
                ):
                    # mJ/ms → mW: same window as pm_period_total_mj (last infer + idle samples)
                    period_ms = period_us_pm * 1e-3
                    p_avg_mw = (state.pm_period_total_mj / period_ms) * 1000.0
                    if p_avg_mw > 0.0:
                        state.battery_p_avg_mw_hist.append(p_avg_mw)
                        state.battery_time_hist.append(now)
                        state.pm_period_mj_hist.append(state.pm_period_total_mj)
                        state.pm_period_mj_time_hist.append(now)
                recorder.record_detection_result(
                    result, state.class_labels, state.cpu_usage_percent
                )

            elif which == "device_info":
                info = dev_msg.device_info
                state.model_name = info.model_name or "unknown"
                state.class_labels = list(info.class_labels)
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
                state.pp_conf_threshold = float(info.pp_conf_threshold)
                state.pp_iou_threshold = float(info.pp_iou_threshold)
                state.track_thresh = float(info.track_thresh)
                state.det_thresh = float(info.det_thresh)
                state.sim1_thresh = float(info.sim1_thresh)
                state.sim2_thresh = float(info.sim2_thresh)
                state.tlost_cnt = int(info.tlost_cnt)
                if state.pp_cfg_pending_device_info > 0:
                    state.pp_cfg_status_text = (
                        state.pp_cfg_status_text.rstrip() + "\nDeviceInfo received."
                    )
                    state.pp_cfg_pending_device_info -= 1
                recorder.record_device_info(info)

            elif which == "ack":
                ack = dev_msg.ack
                state.last_ack = f"ACK success={ack.success}"
                state.last_ack_timestamp = int(ack.timestamp_ms)
                if pending_ack_types:
                    kind = pending_ack_types.popleft()
                    if kind == "display":
                        if pending_display:
                            desired = pending_display.popleft()
                            if ack.success:
                                state.display_enabled = desired
                    elif kind == "pp_config":
                        base = state.pp_cfg_status_text.strip()
                        if not base:
                            base = "Sent set_postprocess_config."
                        state.pp_cfg_status_text = (
                            base
                            + "\n"
                            + f"ACK success={ack.success} (t={int(ack.timestamp_ms)} ms)"
                        )
                elif pending_display:
                    desired = pending_display.popleft()
                    if ack.success:
                        state.display_enabled = desired
                recorder.record_ack(ack)

            elif which == "tof_result":
                tof_result = dev_msg.tof_result
                state.tof_depth_grid = reshape_tof_grid(tof_result.depth_mm)
                state.tof_sigma_grid = reshape_tof_grid(tof_result.range_sigma_mm)
                state.tof_signal_grid = reshape_tof_grid(tof_result.signal_per_spad)
                recorder.record_tof_result(tof_result)

            elif which == "tof_alert_result":
                tof_alert_result = dev_msg.tof_alert_result
                state.person_mm = [int(v) for v in tof_alert_result.person_mm]
                fl = int(tof_alert_result.flags)
                state.tof_alert = bool(fl & TOF_PB_FLAG_ALERT)
                state.tof_stale = bool(fl & TOF_PB_FLAG_STALE)
                recorder.record_tof_alert_result(tof_alert_result)

            elif which == "cpu_usage_sample":
                cpu_sample = dev_msg.cpu_usage_sample
                now = time.time()
                _touch_plot_epoch(state, now)
                state.cpu_usage_percent = float(cpu_sample.cpu_usage_percent)
                state.cpu_hist.append(state.cpu_usage_percent)
                state.cpu_time_hist.append(now)
                recorder.record_cpu_usage_sample(cpu_sample)

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
    recorder: RecordingManager,
) -> None:
    power_link: Optional[PowerLink] = None
    while not stop_evt.is_set():
        try:
            if power_port is None:
                state.power_connected = False
                state.power_port_path = ""
                state.power_baud = 0
                time.sleep(0.5)
                continue

            if power_link is None:
                power_link = PowerLink(power_port, power_baud, timeout_s)
                state.power_connected = True
                state.power_port_path = power_port
                state.power_baud = power_baud
                state.last_power_error = ""

            sample = power_link.recv_power_sample(power_sample_pb2)
            energy_j = float(sample.energy_j)
            duration_us = float(sample.duration_us)
            is_inference = bool(sample.is_inference)
            timestamp_us = int(sample.timestamp_us)

            state.power_in_inference = is_inference
            state.power_timestamp_us = timestamp_us
            now = time.time()
            _touch_plot_epoch(state, now)
            duration_s = duration_us * 1e-6
            avg_mw = (energy_j / duration_s) * 1000.0 if duration_s > 0.0 else 0.0

            energy_mj = energy_j * 1000.0

            if is_inference:
                state.power_infer_hist_mw.append(avg_mw)
                state.power_infer_time_hist.append(now)
                state.power_infer_avg_mw = avg_mw
                state.power_infer_duration_ms = duration_us / 1000.0
                state.power_infer_energy_uj = energy_j * 1e6
                state.pm_last_inf_duration_us = duration_us
                state.pm_avg_inf_mj_hist.append(energy_mj)
                state.pm_avg_inf_mj_time_hist.append(now)
                state.pm_last_inf_mj = energy_mj
                state.pm_seen_infer_mj = True
            else:
                state.power_idle_hist_mw.append(avg_mw)
                state.power_idle_time_hist.append(now)
                state.power_idle_avg_mw = avg_mw
                state.pm_last_idle_duration_us = duration_us
                state.pm_avg_idle_mj_hist.append(energy_mj)
                state.pm_avg_idle_mj_time_hist.append(now)
                state.pm_last_idle_mj = energy_mj
                state.pm_seen_idle_mj = True

            if state.pm_seen_infer_mj and state.pm_seen_idle_mj:
                state.pm_period_total_mj = state.pm_last_inf_mj + state.pm_last_idle_mj
            recorder.record_power_sample(sample)
        except Exception as exc:
            state.last_power_error = str(exc)
            state.power_connected = False
            state.power_port_path = ""
            state.power_baud = 0
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
    state = VisualizerState()
    state.battery_capacity_mah = float(args.battery_mah)
    state.battery_supply_voltage_v = float(args.battery_v)
    recorder = RecordingManager(os.path.join(REPO_ROOT, "visualizer_screenshots"))
    cmd_queue: Queue[tuple[str, Any]] = Queue()
    stop_evt = threading.Event()

    firmware_port_for_power_exclude = resolve_port(args.port)
    rx_thread = threading.Thread(
        target=receiver_loop,
        args=(args.port, args.baud, args.timeout, state, cmd_queue, stop_evt, recorder),
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
