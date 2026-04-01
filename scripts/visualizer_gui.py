"""GUI creation and styling for visualizer."""

from queue import Queue
from typing import Optional

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button, TextBox


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
    C_POWER_PERIOD = "#FFC000"
    C_CPU = "#6AA84F"
    C_BATTERY = "#B565D8"
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

    # --- Power energy (mJ) figure ---
    fig_pm_mj = plt.figure("Power Energy (mJ)", figsize=(8, 5))
    fig_pm_mj.set_facecolor(C_FIG_BG)
    ax_pm_mj = fig_pm_mj.add_subplot(111)

    # --- Battery estimate figure ---
    fig_battery = plt.figure("Battery Life Estimate", figsize=(8, 5))
    fig_battery.set_facecolor(C_FIG_BG)
    fig_battery.subplots_adjust(bottom=0.18)
    ax_battery = fig_battery.add_subplot(111)

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
    (line_power_period,) = ax_power.plot(
        [],
        [],
        label="Period",
        color=C_POWER_PERIOD,
        linewidth=LW,
        linestyle="--",
        zorder=3,
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

    # --- Power energy (mJ) line chart ---
    _style_axis(ax_pm_mj, C_AX_BG)
    (line_pm_inf_mj,) = ax_pm_mj.plot(
        [], [], label="Inference", color=C_POWER_INF, linewidth=LW
    )
    (line_pm_idle_mj,) = ax_pm_mj.plot(
        [], [], label="Idle", color=C_POWER_IDLE, linewidth=LW
    )
    (line_pm_period_mj,) = ax_pm_mj.plot(
        [],
        [],
        label="Period",
        color=C_POWER_PERIOD,
        linewidth=LW,
        linestyle="--",
        zorder=3,
    )
    ax_pm_mj.set_title("Power Energy / Phase", color=C_TITLE, pad=10)
    ax_pm_mj.set_xlabel("Time (s)")
    ax_pm_mj.set_ylabel("mJ")
    _legend(ax_pm_mj, C_AX_BG)
    pm_mj_stats_text = ax_pm_mj.text(
        0.98,
        0.95,
        "",
        transform=ax_pm_mj.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color=C_ACCENT,
    )

    # --- Battery runtime: mAh × V_supply → mWh, then / P_avg mW → h ---
    _style_axis(ax_battery, C_AX_BG)
    (line_battery_hours,) = ax_battery.plot(
        [], [], label="Est. runtime", color=C_BATTERY, linewidth=LW
    )
    ax_battery.set_title(
        "Battery Life Estimate",
        color=C_TITLE,
        pad=10,
    )
    ax_battery.set_xlabel("Time (s)")
    ax_battery.set_ylabel("Hours")
    _legend(ax_battery, C_AX_BG)
    battery_note = ax_battery.text(
        0.02,
        0.02,
        "mWh = mAh × V_supply; hours = mWh / P_avg.  "
        "P_avg = period mJ / (t_infer + t_idle), as mW",
        transform=ax_battery.transAxes,
        ha="left",
        va="bottom",
        fontsize=8,
        color="#9898b8",
    )
    battery_stats = ax_battery.text(
        0.98,
        0.98,
        "",
        transform=ax_battery.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color=C_TITLE,
        linespacing=1.2,
        bbox={
            "boxstyle": "round,pad=0.35",
            "facecolor": "#12121c",
            "edgecolor": "#3a3a5c",
            "alpha": 0.92,
        },
    )
    ax_tb_batt = fig_battery.add_axes([0.12, 0.03, 0.32, 0.065])
    tb_battery = TextBox(
        ax_tb_batt,
        "mAh ",
        initial=f"{state.battery_capacity_mah:g}",
        color="#252545",
        hovercolor="#35355a",
    )
    tb_battery.text_disp.set_color(C_TEXT)
    tb_battery.label.set_color(C_TEXT)

    def on_battery_mah(text: str) -> None:
        try:
            v = float(str(text).strip().replace(",", ""))
            if v > 0.0:
                state.battery_capacity_mah = v
        except ValueError:
            pass

    tb_battery.on_submit(on_battery_mah)

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
    fig_pm_mj.tight_layout()
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
        t0_starts: list[float] = []
        if state.power_infer_time_hist:
            t0_starts.append(state.power_infer_time_hist[0])
        if state.power_idle_time_hist:
            t0_starts.append(state.power_idle_time_hist[0])
        if state.battery_time_hist:
            t0_starts.append(state.battery_time_hist[0])
        t0_power = min(t0_starts) if t0_starts else 0.0

        if state.power_infer_time_hist:
            x_inf = np.array([t - t0_power for t in state.power_infer_time_hist])
        else:
            x_inf = np.array([])
        if state.power_idle_time_hist:
            x_idle = np.array([t - t0_power for t in state.power_idle_time_hist])
        else:
            x_idle = np.array([])
        if state.battery_time_hist and state.battery_p_avg_mw_hist:
            x_period = np.array(
                [t - t0_power for t in state.battery_time_hist]
            )
            y_period = np.array(state.battery_p_avg_mw_hist, dtype=float)
        else:
            x_period = np.array([])
            y_period = np.array([])

        line_power_inf.set_data(x_inf, np.array(state.power_infer_hist_mw))
        line_power_idle.set_data(x_idle, np.array(state.power_idle_hist_mw))
        line_power_period.set_data(x_period, y_period)

        period_mw = (
            float(state.battery_p_avg_mw_hist[-1])
            if state.battery_p_avg_mw_hist
            else None
        )
        if period_mw is not None:
            power_peak_text.set_text(
                f"infer: {state.power_infer_avg_mw:.0f} mW   "
                f"idle: {state.power_idle_avg_mw:.0f} mW   "
                f"period: {period_mw:.0f} mW"
            )
        else:
            power_peak_text.set_text(
                f"infer: {state.power_infer_avg_mw:.0f} mW   "
                f"idle: {state.power_idle_avg_mw:.0f} mW"
            )
        ax_power.relim()
        ax_power.autoscale_view()
        return (line_power_inf, line_power_idle, line_power_period, power_peak_text)

    def update_pm_mj(_frame_idx):
        t0_starts: list[float] = []
        if state.pm_avg_inf_mj_time_hist:
            t0_starts.append(state.pm_avg_inf_mj_time_hist[0])
        if state.pm_avg_idle_mj_time_hist:
            t0_starts.append(state.pm_avg_idle_mj_time_hist[0])
        if state.pm_period_mj_time_hist:
            t0_starts.append(state.pm_period_mj_time_hist[0])
        t0_mj = min(t0_starts) if t0_starts else 0.0

        if state.pm_avg_inf_mj_time_hist:
            x_inf_mj = np.array(
                [t - t0_mj for t in state.pm_avg_inf_mj_time_hist]
            )
        else:
            x_inf_mj = np.array([])
        if state.pm_avg_idle_mj_time_hist:
            x_idle_mj = np.array(
                [t - t0_mj for t in state.pm_avg_idle_mj_time_hist]
            )
        else:
            x_idle_mj = np.array([])
        if state.pm_period_mj_time_hist:
            x_period_mj = np.array(
                [t - t0_mj for t in state.pm_period_mj_time_hist]
            )
            y_period_mj = np.array(state.pm_period_mj_hist, dtype=float)
        else:
            x_period_mj = np.array([])
            y_period_mj = np.array([])

        line_pm_inf_mj.set_data(x_inf_mj, np.array(state.pm_avg_inf_mj_hist))
        line_pm_idle_mj.set_data(x_idle_mj, np.array(state.pm_avg_idle_mj_hist))
        line_pm_period_mj.set_data(x_period_mj, y_period_mj)
        if state.pm_seen_infer_mj and state.pm_seen_idle_mj:
            pm_mj_stats_text.set_text(
                f"infer: {state.pm_last_inf_mj:.2f} mJ   "
                f"idle: {state.pm_last_idle_mj:.2f} mJ   "
                f"period: {state.pm_period_total_mj:.2f} mJ"
            )
        else:
            parts: list[str] = []
            if state.pm_seen_infer_mj:
                parts.append(f"infer: {state.pm_last_inf_mj:.2f} mJ")
            if state.pm_seen_idle_mj:
                parts.append(f"idle: {state.pm_last_idle_mj:.2f} mJ")
            pm_mj_stats_text.set_text("   ".join(parts))
        ax_pm_mj.relim()
        ax_pm_mj.autoscale_view()
        return (line_pm_inf_mj, line_pm_idle_mj, line_pm_period_mj, pm_mj_stats_text)

    def update_battery(_frame_idx):
        cap_mah = float(state.battery_capacity_mah)
        v_supply = float(state.battery_supply_voltage_v)
        energy_mwh = cap_mah * v_supply
        if state.battery_time_hist and state.battery_p_avg_mw_hist:
            t0 = state.battery_time_hist[0]
            x = np.array([t - t0 for t in state.battery_time_hist])
            p = np.array(state.battery_p_avg_mw_hist, dtype=float)
            hours = energy_mwh / p
            line_battery_hours.set_data(x, hours)
            last_h = float(hours[-1])
            last_p = float(p[-1])
            battery_stats.set_text(
                f"{cap_mah:g} mAh × {v_supply:g} V → {energy_mwh:g} mWh\n"
                f"÷ {last_p:.0f} mW ≈ {last_h:.2f} h"
            )
        else:
            line_battery_hours.set_data([], [])
            battery_stats.set_text("—\n(need power link + frames)")
        ax_battery.relim()
        ax_battery.autoscale_view()
        return (line_battery_hours, battery_stats)

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
    anim_pm_mj = FuncAnimation(
        fig_pm_mj, update_pm_mj, interval=120, blit=False, cache_frame_data=False
    )
    anim_battery = FuncAnimation(
        fig_battery, update_battery, interval=120, blit=False, cache_frame_data=False
    )
    anim_info = FuncAnimation(
        fig_info, update_info, interval=120, blit=False, cache_frame_data=False
    )

    figures = [
        fig_timing,
        fig_cpu,
        fig_tof,
        fig_power,
        fig_pm_mj,
        fig_battery,
        fig_info,
    ]
    animations = [
        anim_timing,
        anim_cpu,
        anim_tof,
        anim_power,
        anim_pm_mj,
        anim_battery,
        anim_info,
    ]

    _closing_all = [False]

    def on_any_figure_close(_event) -> None:
        if _closing_all[0]:
            return
        _closing_all[0] = True
        plt.close("all")

    for fig in figures:
        fig.canvas.mpl_connect("close_event", on_any_figure_close)

    return figures, animations
