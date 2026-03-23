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
  display_enabled: Optional[bool] = None
  firmware_recognized: bool = False
  host_introduced: bool = False

  infer_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
  post_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
  period_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))
  fps_hist: deque[float] = field(default_factory=lambda: deque(maxlen=240))

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
  last_rx_time: float = 0.0


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
    self._tx_lock = threading.Lock()

  def close(self) -> None:
    self._ser.close()

  def recv_device_message(self) -> messages_pb2.DeviceMessage:
    length = struct.unpack("<I", read_exact(self._ser, 4))[0]
    if length == 0 or length > 16 * 1024:
      raise ValueError(f"Invalid frame length {length}")
    payload = read_exact(self._ser, length)
    msg = messages_pb2.DeviceMessage()
    msg.ParseFromString(payload)
    return msg

  def send_host_message(self, msg: messages_pb2.HostMessage) -> None:
    payload = msg.SerializeToString()
    frame = struct.pack("<I", len(payload)) + payload
    with self._tx_lock:
      self._ser.write(frame)
      self._ser.flush()


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
    link: SerialLink,
    state: VisualizerState,
    cmd_queue: Queue[tuple[str, Optional[bool]]],
    stop_evt: threading.Event,
) -> None:
  command_id = 1
  while not stop_evt.is_set():
    try:
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

        if result.HasField("tof"):
          tof = result.tof
          state.hand_mm = int(tof.hand_distance_mm)
          state.hazard_mm = int(tof.hazard_distance_mm)
          state.distance_3d_mm = float(tof.distance_3d_mm)
          state.tof_alert = bool(tof.alert)
          state.tof_stale = bool(tof.stale)
          if len(tof.depth_grid) == 64:
            state.tof_grid = np.array(tof.depth_grid, dtype=np.float32).reshape(8, 8)
          else:
            state.tof_grid = np.full((8, 8), np.nan, dtype=np.float32)

      elif which == "device_info":
        info = dev_msg.device_info
        state.model_name = info.model_name or "unknown"
        state.class_labels = list(info.class_labels)
        state.firmware_recognized = True

      elif which == "ack":
        ack = dev_msg.ack
        state.last_ack = f"ACK command_id={ack.command_id} success={ack.success}"

    except Exception as exc:  # Broad catch keeps GUI alive while cable reconnects.
      state.last_error = str(exc)
      time.sleep(0.05)


def create_gui(
    state: VisualizerState, cmd_queue: Queue[tuple[str, Optional[bool]]]
) -> tuple[plt.Figure, FuncAnimation]:
  fig = plt.figure("FYP Firmware Serial Visualizer", figsize=(12, 8))
  grid = fig.add_gridspec(3, 4)

  ax_timing = fig.add_subplot(grid[0:2, 0:2])
  ax_tof = fig.add_subplot(grid[0:2, 2:4])
  ax_text = fig.add_subplot(grid[2, 0:3])
  ax_text.axis("off")

  ax_btn_info = fig.add_subplot(grid[2, 3])
  ax_btn_toggle = fig.add_axes([0.76, 0.06, 0.20, 0.06])
  btn_info = Button(ax_btn_info, "Get Device Info")
  btn_toggle = Button(ax_btn_toggle, "Toggle Display")

  (line_inf,) = ax_timing.plot([], [], label="Inference (ms)")
  (line_pp,) = ax_timing.plot([], [], label="Postprocess (ms)")
  (line_period,) = ax_timing.plot([], [], label="NN Period (ms)")
  ax_timing.set_title("Timing")
  ax_timing.set_xlabel("Frame")
  ax_timing.set_ylabel("Milliseconds")
  ax_timing.legend(loc="upper right")
  ax_timing.grid(True)

  img = ax_tof.imshow(
      np.full((8, 8), np.nan), cmap="turbo", interpolation="nearest", vmin=0, vmax=2000
  )
  fig.colorbar(img, ax=ax_tof, label="Distance (mm)")
  ax_tof.set_title("ToF Depth Grid")

  text_box = ax_text.text(0.0, 1.0, "", va="top", ha="left", family="monospace")

  display_toggle_state = {"enabled": True}

  def on_get_info(_event) -> None:
    cmd_queue.put(("get_info", None))

  def on_toggle_display(_event) -> None:
    display_toggle_state["enabled"] = not display_toggle_state["enabled"]
    cmd_queue.put(("set_display", display_toggle_state["enabled"]))

  btn_info.on_clicked(on_get_info)
  btn_toggle.on_clicked(on_toggle_display)

  def update(_frame_idx):
    x = np.arange(len(state.infer_hist))
    line_inf.set_data(x, np.array(state.infer_hist))
    line_pp.set_data(x, np.array(state.post_hist))
    line_period.set_data(x, np.array(state.period_hist))

    ax_timing.relim()
    ax_timing.autoscale_view()

    img.set_data(state.tof_grid)

    fps = state.fps_hist[-1] if state.fps_hist else 0.0
    status = "ALERT" if state.tof_alert else "OK"
    stale = "stale" if state.tof_stale else "fresh"
    summary = (
        f"Frames: {state.frame_count}    Last timestamp: {state.last_timestamp} ms    "
        f"Detections: {state.detection_count}\n"
        f"Model: {state.model_name}    Class labels: {', '.join(state.class_labels) or '-'}\n"
        f"ToF: hand={state.hand_mm} mm  hazard={state.hazard_mm} mm  "
        f"distance_3d={state.distance_3d_mm:0.1f} mm  status={status}/{stale}\n"
        f"Perf: fps={fps:0.1f}    Last ACK: {state.last_ack}\n"
        f"Recognition: host->fw={'yes' if state.host_introduced else 'no'}  "
        f"fw->host={'yes' if state.firmware_recognized else 'no'}\n"
        f"Last error: {state.last_error or '-'}\n"
        "Detections:\n"
        f"{state.detections_text}"
    )
    text_box.set_text(summary)

    return line_inf, line_pp, line_period, img, text_box

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

  link = SerialLink(resolve_port(args.port), args.baud, args.timeout)
  rx_thread = threading.Thread(
      target=receiver_loop, args=(link, state, cmd_queue, stop_evt), daemon=True
  )
  rx_thread.start()

  # Handshake: host introduces itself by requesting device metadata.
  cmd_queue.put(("get_info", None))

  fig, _anim = create_gui(state, cmd_queue)
  try:
    plt.show()
  finally:
    stop_evt.set()
    link.close()
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
