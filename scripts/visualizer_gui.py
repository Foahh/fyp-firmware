"""GUI creation and styling for visualizer."""

from queue import Queue
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button


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
    leg = ax.legend(
        loc="upper left", fontsize=9, framealpha=0.85, edgecolor="none", facecolor=bg
    )
    for text in leg.get_texts():
        text.set_color("#d0d0e0")


def create_gui(
    state,
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
    plt.rcParams.update(
        {
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
        }
    )

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
    btn_toggle = Button(
        btn_ax2, "Toggle Display", color="#252545", hovercolor="#35355a"
    )
    for btn in (btn_info, btn_toggle):
        btn.label.set_color(C_TEXT)
        btn.label.set_fontsize(10)

    # --- Timing plot ---
    _style_axis(ax_timing, C_AX_BG)
    (line_inf,) = ax_timing.plot([], [], label="Inference", color=C_INFER, linewidth=LW)
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
    (line_cpu,) = ax_cpu.plot([], [], label="CPU %", color=C_CPU, linewidth=LW)
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
            cpu_freq_text.set_text(
                f"{cpu_usage_mhz:.0f} MHz  |  CPU {mcu_mhz} MHz / NPU {npu} MHz"
            )
        else:
            cpu_freq_text.set_text(f"CPU {mcu_mhz} MHz / NPU {npu} MHz")
        return (line_cpu, cpu_percent_text, cpu_freq_text)

    def update_tof(_frame_idx):
        img.set_data(state.tof_grid)
        return (img,)

    def update_power(_frame_idx):
        t0_power = min(
            state.power_infer_time_hist[0]
            if state.power_infer_time_hist
            else float("inf"),
            state.power_idle_time_hist[0]
            if state.power_idle_time_hist
            else float("inf"),
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
        power_peak_text.set_text(
            f"infer: {state.power_infer_avg_mw:.0f} mW   idle: {state.power_idle_avg_mw:.0f} mW"
        )
        ax_power.relim()
        ax_power.autoscale_view()
        return (line_power_inf, line_power_idle, power_peak_text)

    def update_info(_frame_idx):
        status = "ALERT" if state.tof_alert else "OK"
        stale = "stale" if state.tof_stale else "fresh"
        class_labels = ", ".join(state.class_labels) or "-"
        recog_host_fw = "yes" if state.host_introduced else "no"
        recog_fw_host = "yes" if state.firmware_recognized else "no"
        fw_path = state.firmware_port_path or "-"
        fw_baud = f"{state.firmware_baud}" if state.firmware_baud else "-"
        detections_text = state.detections_text or "  -"
        last_pp = state.post_hist[-1] if state.post_hist else 0.0
        last_trk = state.tracker_hist[-1] if state.tracker_hist else 0.0
        last_fps = state.fps_hist[-1] if state.fps_hist else 0.0
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
            f"  STM32   port {fw_path} @ {fw_baud} bps",
            f"          serial open: {'yes' if state.firmware_connected else 'no'}   "
            f"H\u2192D: {recog_host_fw}   D\u2192H: {recog_fw_host}  (need yes/yes for traffic)",
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
            f"  Frames  {state.frame_count}   Drops: {state.frame_drops}   FPS: {last_fps:.1f}",
            f"  Timing  PP: {last_pp:.0f} us   Trk: {last_trk:.0f} us",
            f"  Timestamp  msg: {state.last_timestamp} ms   detection: {state.detection_timestamp} ms   info: {state.device_info_timestamp} ms   ack: {state.last_ack_timestamp} ms",
            f"  Labels  {class_labels}",
            f"  ACK     {state.last_ack}",
            f"  Err     {state.last_error or '-'}",
            f"  PwrErr  {state.last_power_error or '-'}",
            "",
            f" Detections ({state.detection_count})   Tracked ({state.tracked_box_count})",
            SEP,
            f"  {detections_text}",
        ]
        text_box.set_text("\n".join(lines))
        return (text_box,)

    anim_timing = FuncAnimation(
        fig_timing, update_timing, interval=120, blit=False, cache_frame_data=False
    )
    anim_cpu = FuncAnimation(
        fig_cpu, update_cpu, interval=120, blit=False, cache_frame_data=False
    )
    anim_tof = FuncAnimation(
        fig_tof, update_tof, interval=120, blit=False, cache_frame_data=False
    )
    anim_power = FuncAnimation(
        fig_power, update_power, interval=120, blit=False, cache_frame_data=False
    )
    anim_info = FuncAnimation(
        fig_info, update_info, interval=120, blit=False, cache_frame_data=False
    )

    figures = [fig_timing, fig_cpu, fig_tof, fig_power, fig_info]
    animations = [anim_timing, anim_cpu, anim_tof, anim_power, anim_info]

    _closing_all = [False]

    def on_any_figure_close(_event) -> None:
        if _closing_all[0]:
            return
        _closing_all[0] = True
        plt.close("all")

    for fig in figures:
        fig.canvas.mpl_connect("close_event", on_any_figure_close)

    return figures, animations
