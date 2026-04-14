"""State models and pure helpers for the visualizer."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field
from typing import Any, Optional

import numpy as np

try:
    from .visualizer_proto import messages_pb2
except ImportError:
    from visualizer_proto import messages_pb2

HISTORY_LEN = 240


def _history() -> deque[float]:
    return deque(maxlen=HISTORY_LEN)


@dataclass(frozen=True)
class DetBox:
    """Normalized detection in NN input space (center x,y, size w,h), same as firmware."""

    cx: float
    cy: float
    w: float
    h: float
    score: float
    class_id: int


@dataclass(frozen=True)
class TrkBox:
    """Normalized tracked box (center x,y, size w,h)."""

    cx: float
    cy: float
    w: float
    h: float
    track_id: int


@dataclass
class VisualizerState:
    frame_count: int = 0
    last_timestamp: int = 0
    detection_timestamp: int = 0
    detection_count: int = 0
    tracked_box_count: int = 0
    detections_text: str = "No detections yet."
    detection_boxes: list[DetBox] = field(default_factory=list)
    track_boxes: list[TrkBox] = field(default_factory=list)

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
    pp_cfg_status_text: str = ""
    pp_cfg_pending_device_info: int = 0

    nn_idle_us: int = 0
    frame_drops: int = 0
    cpu_usage_percent: float = 0.0
    cpu_hist: deque[float] = field(default_factory=_history)
    cpu_time_hist: deque[float] = field(default_factory=_history)

    infer_hist: deque[float] = field(default_factory=_history)
    post_hist: deque[float] = field(default_factory=_history)
    tracker_hist: deque[float] = field(default_factory=_history)
    period_hist: deque[float] = field(default_factory=_history)
    fps_hist: deque[float] = field(default_factory=_history)
    timing_time_hist: deque[float] = field(default_factory=_history)

    track_person_mm: dict[int, int] = field(default_factory=dict)
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
    power_infer_hist_mw: deque[float] = field(default_factory=_history)
    power_infer_time_hist: deque[float] = field(default_factory=_history)
    power_idle_hist_mw: deque[float] = field(default_factory=_history)
    power_idle_time_hist: deque[float] = field(default_factory=_history)

    pm_avg_inf_mj_hist: deque[float] = field(default_factory=_history)
    pm_avg_inf_mj_time_hist: deque[float] = field(default_factory=_history)
    pm_avg_idle_mj_hist: deque[float] = field(default_factory=_history)
    pm_avg_idle_mj_time_hist: deque[float] = field(default_factory=_history)
    pm_last_inf_mj: float = 0.0
    pm_last_idle_mj: float = 0.0
    pm_last_inf_duration_us: float = 0.0
    pm_last_idle_duration_us: float = 0.0
    pm_period_total_mj: float = 0.0
    pm_seen_infer_mj: bool = False
    pm_seen_idle_mj: bool = False
    pm_period_mj_hist: deque[float] = field(default_factory=_history)
    pm_period_mj_time_hist: deque[float] = field(default_factory=_history)

    battery_capacity_mah: float = 820.0
    battery_supply_voltage_v: float = 5.0
    battery_p_avg_mw_hist: deque[float] = field(default_factory=_history)
    battery_time_hist: deque[float] = field(default_factory=_history)


def build_detection_text(
    result: messages_pb2.DetectionResult, class_labels: list[str]
) -> str:
    if not result.detections:
        return "No detections."

    lines = []
    for idx, det in enumerate(result.detections):
        cid = int(det.class_id)
        class_name = class_labels[cid] if 0 <= cid < len(class_labels) else str(cid)
        lines.append(
            f"#{idx} {class_name:>10}  conf={det.score:0.2f}  "
            f"xywh=({det.x:0.2f}, {det.y:0.2f}, {det.w:0.2f}, {det.h:0.2f})"
        )
    return "\n".join(lines)


def extract_detection_boxes(
    result: messages_pb2.DetectionResult,
) -> list[DetBox]:
    return [
        DetBox(
            float(det.x),
            float(det.y),
            float(det.w),
            float(det.h),
            float(det.score),
            int(det.class_id),
        )
        for det in result.detections
    ]


def extract_track_boxes(result: messages_pb2.DetectionResult) -> list[TrkBox]:
    return [
        TrkBox(
            float(track.x),
            float(track.y),
            float(track.w),
            float(track.h),
            int(track.track_id),
        )
        for track in result.tracks
    ]


def touch_plot_epoch(state: VisualizerState, now: float) -> None:
    if state.plot_epoch_s is None:
        state.plot_epoch_s = now


def reshape_tof_grid(values: Any) -> np.ndarray:
    if len(values) != 64:
        return np.full((8, 8), np.nan, dtype=np.float32)
    return np.array(values, dtype=np.float32).reshape(8, 8)
