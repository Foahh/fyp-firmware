"""Runtime receiver loops for firmware and power-monitor streams."""

from __future__ import annotations

import sys
import threading
import time
from collections import deque
from queue import Empty, Queue
from typing import Any, Optional

try:
    from .visualizer_proto import (
        TOF_PB_FLAG_ALERT,
        TOF_PB_FLAG_STALE,
        messages_pb2,
        power_sample_pb2,
    )
    from .visualizer_recording import RecordingManager
    from .visualizer_serial import PowerLink, SerialLink
    from .visualizer_state import (
        VisualizerState,
        build_detection_text,
        extract_detection_boxes,
        extract_track_boxes,
        reshape_tof_grid,
        touch_plot_epoch,
    )
except ImportError:
    from visualizer_proto import (
        TOF_PB_FLAG_ALERT,
        TOF_PB_FLAG_STALE,
        messages_pb2,
        power_sample_pb2,
    )
    from visualizer_recording import RecordingManager
    from visualizer_serial import PowerLink, SerialLink
    from visualizer_state import (
        VisualizerState,
        build_detection_text,
        extract_detection_boxes,
        extract_track_boxes,
        reshape_tof_grid,
        touch_plot_epoch,
    )

GET_INFO_RESEND_S = 1.5


def _send_get_info(link: SerialLink, state: VisualizerState, why: str) -> float:
    msg = messages_pb2.HostMessage()
    msg.get_device_info.SetInParent()
    msg.get_device_info.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
    link.send_host_message(msg)
    state.host_introduced = True
    print(f"[firmware] Sent get_device_info ({why})", file=sys.stderr)
    return time.monotonic()


def _drain_host_commands(
    link: SerialLink,
    state: VisualizerState,
    cmd_queue: Queue[tuple[str, Any]],
    pending_display: deque[bool],
    pending_ack_types: deque[str],
    last_get_info_mono: float,
) -> float:
    while True:
        try:
            command, value = cmd_queue.get_nowait()
        except Empty:
            return last_get_info_mono

        msg = messages_pb2.HostMessage()
        if command == "get_info":
            msg.get_device_info.SetInParent()
            msg.get_device_info.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
            state.host_introduced = True
            last_get_info_mono = time.monotonic()
        elif command == "set_display":
            msg.set_display_enabled.enabled = bool(value)
            msg.set_display_enabled.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
            state.host_introduced = True
            pending_display.append(bool(value))
            pending_ack_types.append("display")
        elif command == "set_app_config" and isinstance(value, dict):
            cfg = msg.set_app_config
            cfg.pp_conf_threshold = float(value["pp_conf_threshold"])
            cfg.pp_iou_threshold = float(value["pp_iou_threshold"])
            cfg.track_thresh = float(value["track_thresh"])
            cfg.det_thresh = float(value["det_thresh"])
            cfg.sim1_thresh = float(value["sim1_thresh"])
            cfg.sim2_thresh = float(value["sim2_thresh"])
            cfg.tlost_cnt = int(value["tlost_cnt"])
            cfg.alert_threshold_mm = int(value["alert_threshold_mm"])
            cfg.timestamp_ms = int(time.time() * 1000) & 0xFFFFFFFF
            state.host_introduced = True
            pending_ack_types.append("app_config")
        else:
            continue

        link.send_host_message(msg)
        if command == "get_info":
            print("[firmware] Sent get_device_info (queued)", file=sys.stderr)


def _record_period_metrics(state: VisualizerState, now: float) -> None:
    period_us_pm = state.pm_last_inf_duration_us + state.pm_last_idle_duration_us
    if (
        period_us_pm <= 0.0
        or not state.pm_seen_infer_mj
        or not state.pm_seen_idle_mj
        or state.pm_period_total_mj <= 0.0
    ):
        return

    period_ms = period_us_pm * 1e-3
    p_avg_mw = (state.pm_period_total_mj / period_ms) * 1000.0
    if p_avg_mw <= 0.0:
        return

    state.battery_p_avg_mw_hist.append(p_avg_mw)
    state.battery_time_hist.append(now)
    state.pm_period_mj_hist.append(state.pm_period_total_mj)
    state.pm_period_mj_time_hist.append(now)


def _handle_detection_result(
    state: VisualizerState,
    result: messages_pb2.DetectionResult,
    recorder: RecordingManager,
) -> None:
    state.frame_count += 1
    state.last_timestamp = int(result.sent_timestamp_ms)
    state.detection_timestamp = int(result.frame_timestamp_ms)
    state.detection_count = len(result.detections)
    state.tracked_box_count = len(result.tracks)
    state.detections_text = build_detection_text(result, state.class_labels)
    state.detection_boxes = extract_detection_boxes(result)
    state.track_boxes = extract_track_boxes(result)

    period_us = float(result.nn_period_us)
    fps = (1_000_000.0 / period_us) if period_us > 0.0 else 0.0
    now = time.time()
    touch_plot_epoch(state, now)
    state.infer_hist.append(float(result.inference_us))
    state.post_hist.append(float(result.postprocess_us))
    state.tracker_hist.append(float(result.tracker_us))
    state.period_hist.append(period_us)
    state.fps_hist.append(fps)
    state.timing_time_hist.append(now)
    state.nn_idle_us = int(result.nn_idle_us)
    state.frame_drops = int(result.frame_drop_count)
    _record_period_metrics(state, now)
    recorder.record_detection_result(result, state)


def _handle_device_info(
    state: VisualizerState,
    info: messages_pb2.DeviceInfo,
    recorder: RecordingManager,
) -> None:
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
    state.nn_size_text = (
        f"{info.nn_width_px}x{info.nn_height_px} ({info.nn_input_size_bytes} B)"
        if info.nn_width_px and info.nn_height_px
        else "unknown"
    )
    if info.mcu_freq_mhz >= 800:
        state.build_mode_text = "overdrive"
    elif info.mcu_freq_mhz <= 400:
        state.build_mode_text = "underdrive"
    else:
        state.build_mode_text = "nominal"
    state.camera_mode_text = "snapshot" if info.camera_fps <= 0 else f"{info.camera_fps} fps"
    state.firmware_recognized = True
    state.build_timestamp = info.build_timestamp or "unknown"
    state.pp_conf_threshold = float(info.pp_conf_threshold)
    state.pp_iou_threshold = float(info.pp_iou_threshold)
    state.track_thresh = float(info.track_thresh)
    state.det_thresh = float(info.det_thresh)
    state.sim1_thresh = float(info.sim1_thresh)
    state.sim2_thresh = float(info.sim2_thresh)
    state.tlost_cnt = int(info.tlost_cnt)
    state.alert_threshold_mm = int(info.alert_threshold_mm)
    if state.pp_cfg_pending_device_info > 0:
        state.pp_cfg_status_text = (
            state.pp_cfg_status_text.rstrip() + "\nDeviceInfo received."
        )
        state.pp_cfg_pending_device_info -= 1
    recorder.record_device_info(info)


def _handle_ack(
    state: VisualizerState,
    ack: messages_pb2.Ack,
    pending_display: deque[bool],
    pending_ack_types: deque[str],
    recorder: RecordingManager,
) -> None:
    state.last_ack = f"ACK success={ack.success}"
    state.last_ack_timestamp = int(ack.timestamp_ms)
    if pending_ack_types:
        kind = pending_ack_types.popleft()
        if kind == "display":
            if pending_display:
                desired = pending_display.popleft()
                if ack.success:
                    state.display_enabled = desired
        elif kind == "app_config":
            base = state.pp_cfg_status_text.strip() or "Sent set_app_config."
            state.pp_cfg_status_text = (
                base + "\n" + f"ACK success={ack.success} (t={int(ack.timestamp_ms)} ms)"
            )
    elif pending_display:
        desired = pending_display.popleft()
        if ack.success:
            state.display_enabled = desired
    recorder.record_ack(ack)


def _handle_tof_result(
    state: VisualizerState,
    tof_result: messages_pb2.TofResult,
    recorder: RecordingManager,
) -> None:
    state.tof_depth_grid = reshape_tof_grid(tof_result.depth_mm)
    state.tof_sigma_grid = reshape_tof_grid(tof_result.range_sigma_mm)
    state.tof_signal_grid = reshape_tof_grid(tof_result.signal_per_spad)
    recorder.record_tof_result(tof_result)


def _handle_tof_alert_result(
    state: VisualizerState,
    tof_alert_result: messages_pb2.TofAlertResult,
    recorder: RecordingManager,
) -> None:
    state.track_person_mm = {
        int(person_distance.track_id): int(person_distance.distance_mm)
        for person_distance in tof_alert_result.person_distances
        if int(person_distance.track_id) > 0 and int(person_distance.distance_mm) > 0
    }
    flags = int(tof_alert_result.flags)
    state.tof_alert = bool(flags & TOF_PB_FLAG_ALERT)
    state.tof_stale = bool(flags & TOF_PB_FLAG_STALE)
    recorder.record_tof_alert_result(tof_alert_result)


def _handle_cpu_usage_sample(
    state: VisualizerState,
    cpu_sample: messages_pb2.CpuUsageSample,
    recorder: RecordingManager,
) -> None:
    now = time.time()
    touch_plot_epoch(state, now)
    state.cpu_usage_percent = float(cpu_sample.cpu_usage_percent)
    state.cpu_hist.append(state.cpu_usage_percent)
    state.cpu_time_hist.append(now)
    recorder.record_cpu_usage_sample(cpu_sample)


def receiver_loop(
    port: Optional[str],
    baud: int,
    timeout_s: float,
    state: VisualizerState,
    resolve_port_fn,
    cmd_queue: Queue[tuple[str, Any]],
    stop_evt: threading.Event,
    recorder: RecordingManager,
) -> None:
    pending_display: deque[bool] = deque()
    pending_ack_types: deque[str] = deque()
    link: Optional[SerialLink] = None
    last_get_info_mono = 0.0

    while not stop_evt.is_set():
        try:
            if link is None:
                selected_port = resolve_port_fn(port)
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
                last_get_info_mono = _send_get_info(link, state, "initial handshake")

            last_get_info_mono = _drain_host_commands(
                link,
                state,
                cmd_queue,
                pending_display,
                pending_ack_types,
                last_get_info_mono,
            )
            if not state.firmware_recognized:
                now = time.monotonic()
                if now - last_get_info_mono >= GET_INFO_RESEND_S:
                    last_get_info_mono = _send_get_info(
                        link,
                        state,
                        "retry until DeviceInfo received",
                    )

            dev_msg = link.recv_device_message(messages_pb2)
            state.last_rx_time = time.time()
            which = dev_msg.WhichOneof("payload")

            if which == "detection_result":
                _handle_detection_result(state, dev_msg.detection_result, recorder)
            elif which == "device_info":
                _handle_device_info(state, dev_msg.device_info, recorder)
            elif which == "ack":
                _handle_ack(
                    state,
                    dev_msg.ack,
                    pending_display,
                    pending_ack_types,
                    recorder,
                )
            elif which == "tof_result":
                _handle_tof_result(state, dev_msg.tof_result, recorder)
            elif which == "tof_alert_result":
                _handle_tof_alert_result(state, dev_msg.tof_alert_result, recorder)
            elif which == "cpu_usage_sample":
                _handle_cpu_usage_sample(state, dev_msg.cpu_usage_sample, recorder)
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

            state.power_in_inference = is_inference
            state.power_timestamp_us = int(sample.timestamp_us)
            now = time.time()
            touch_plot_epoch(state, now)
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
