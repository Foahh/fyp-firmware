"""Recording and CSV export for the visualizer."""

from __future__ import annotations

import csv
import os
import threading
import time
from datetime import datetime
from typing import Any, Optional

try:
    from .visualizer_proto import (
        TOF_PB_FLAG_ALERT,
        TOF_PB_FLAG_STALE,
        messages_pb2,
        power_sample_pb2,
    )
    from .visualizer_state import VisualizerState
except ImportError:
    from visualizer_proto import (
        TOF_PB_FLAG_ALERT,
        TOF_PB_FLAG_STALE,
        messages_pb2,
        power_sample_pb2,
    )
    from visualizer_state import VisualizerState


def _iso_timestamp(timestamp_s: float) -> str:
    return datetime.fromtimestamp(timestamp_s).astimezone().isoformat(timespec="seconds")


def _flatten_sequence(prefix: str, values: list[Any], width: int) -> dict[str, Any]:
    row = {}
    padded = list(values[:width]) + [""] * max(0, width - len(values))
    for idx, value in enumerate(padded):
        row[f"{prefix}_{idx:02d}"] = value
    return row


DETECTION_RESULT_FIELDS = [
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
    "nn_idle_us",
    "fps",
    "frame_drop_count",
    "detection_count",
    "tracked_box_count",
]

DETECTION_ITEM_FIELDS = [
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
]

TRACK_FIELDS = [
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
]

DEVICE_INFO_FIELDS = [
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
]

ACK_FIELDS = [
    "host_time_s",
    "record_elapsed_s",
    "recorded_at_iso",
    "success",
    "timestamp_ms",
]

TOF_FIELDS = [
    "host_time_s",
    "record_elapsed_s",
    "recorded_at_iso",
    "timestamp_ms",
    "tof_period_us",
    *[f"depth_mm_{idx:02d}" for idx in range(64)],
    *[f"range_sigma_mm_{idx:02d}" for idx in range(64)],
    *[f"signal_per_spad_{idx:02d}" for idx in range(64)],
]

TOF_ALERT_FIELDS = [
    "host_time_s",
    "record_elapsed_s",
    "recorded_at_iso",
    "timestamp_ms",
    "flags",
    "alert",
    "stale",
    "fusion_period_us",
    "person_count",
    *[f"person_track_id_{idx:02d}" for idx in range(4)],
    *[f"person_distance_mm_{idx:02d}" for idx in range(4)],
]

CPU_FIELDS = [
    "host_time_s",
    "record_elapsed_s",
    "recorded_at_iso",
    "timestamp_ms",
    "cpu_usage_percent",
]

POWER_FIELDS = [
    "host_time_s",
    "record_elapsed_s",
    "recorded_at_iso",
    "timestamp_us",
    "energy_j",
    "energy_mj",
    "duration_us",
    "is_inference",
    "average_mw",
]


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
                    "nn_idle_us": int(result.nn_idle_us),
                    "fps": f"{fps:.6f}",
                    "frame_drop_count": int(result.frame_drop_count),
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
                "person_count": len(tof_alert_result.person_distances),
            }
            row.update(
                _flatten_sequence(
                    "person_track_id",
                    [int(item.track_id) for item in tof_alert_result.person_distances],
                    4,
                )
            )
            row.update(
                _flatten_sequence(
                    "person_distance_mm",
                    [int(item.distance_mm) for item in tof_alert_result.person_distances],
                    4,
                )
            )
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
            DETECTION_RESULT_FIELDS,
            snapshot["detection_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "detections.csv"),
            DETECTION_ITEM_FIELDS,
            snapshot["detection_item_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "tracks.csv"),
            TRACK_FIELDS,
            snapshot["track_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "device_info.csv"),
            DEVICE_INFO_FIELDS,
            snapshot["device_info_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "ack.csv"),
            ACK_FIELDS,
            snapshot["ack_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "tof_result.csv"),
            TOF_FIELDS,
            snapshot["tof_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "tof_alert_result.csv"),
            TOF_ALERT_FIELDS,
            snapshot["tof_alert_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "cpu_usage.csv"),
            CPU_FIELDS,
            snapshot["cpu_rows"],
        )
        self._write_csv(
            os.path.join(session_dir, "power_sample.csv"),
            POWER_FIELDS,
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
