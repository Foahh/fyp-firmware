"""GUI creation and styling for the visualizer."""

from __future__ import annotations

import os
import re
from dataclasses import dataclass
from datetime import datetime
from queue import Queue
from typing import Any

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
from matplotlib.lines import Line2D
from matplotlib.patches import Rectangle
from matplotlib.widgets import Button, TextBox


@dataclass(frozen=True)
class Theme:
    fig_bg: str = "#000000"
    ax_bg: str = "#0a0a14"
    text: str = "#d8d8e8"
    title: str = "#e8e8f0"
    accent: str = "#ED7D31"
    infer: str = "#5B9BD5"
    period: str = "#ED7D31"
    power_period: str = "#FFC000"
    power_mj: str = "#4ECDC4"
    cpu: str = "#6AA84F"
    battery: str = "#B565D8"
    track_box: str = "#70AD47"
    muted: str = "#9898b8"
    legend_text: str = "#d0d0e0"
    spine: str = "#3a3a5c"
    tick: str = "#b0b0c8"
    widget_bg: str = "#252545"
    widget_hover: str = "#35355a"
    record_off: str = "#7a2f2f"
    record_off_hover: str = "#964141"
    record_on: str = "#2f7a43"
    record_on_hover: str = "#3c9554"
    panel_box: str = "#12121c"
    linewidth: float = 2.2


@dataclass
class TimingPanel:
    fig: plt.Figure
    ax: Any
    line_inf: Any
    line_period: Any


@dataclass
class CpuPanel:
    fig: plt.Figure
    ax: Any
    line_cpu: Any
    cpu_percent_text: Any
    cpu_freq_text: Any


@dataclass
class TofPanel:
    fig: plt.Figure
    img_depth: Any
    img_sigma: Any
    img_signal: Any


@dataclass
class PowerPanel:
    fig: plt.Figure
    ax: Any
    line_period: Any
    peak_text: Any


@dataclass
class EnergyPanel:
    fig: plt.Figure
    ax: Any
    line_period_mj: Any
    stats_text: Any


@dataclass
class BatteryPanel:
    fig: plt.Figure
    ax: Any
    line_hours: Any
    stats_text: Any
    battery_box: TextBox


@dataclass
class InfoPanel:
    fig: plt.Figure
    text_box: Any
    status_text: Any
    btn_info: Button
    btn_toggle: Button
    btn_record: Button
    btn_save_all: Button


@dataclass
class ConfigPanel:
    fig: plt.Figure
    status_text: Any
    textboxes: dict[str, TextBox]
    btn_load: Button
    btn_apply: Button


@dataclass
class BboxPanel:
    fig: plt.Figure
    ax: Any


def _series_secs(state, abs_times) -> np.ndarray:
    """Seconds on plot x-axis; shared origin with other charts (state.plot_epoch_s)."""
    if not abs_times:
        return np.array([])
    t0 = state.plot_epoch_s
    if t0 is None:
        t0 = abs_times[0]
    return np.array([float(t) - float(t0) for t in abs_times])


def _style_axis(ax, bg: str, theme: Theme) -> None:
    ax.set_facecolor(bg)
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)
    for spine in ("bottom", "left"):
        ax.spines[spine].set_color(theme.spine)
    ax.tick_params(colors=theme.tick, labelsize=9)
    ax.grid(True, linestyle=":", alpha=0.15, color="white")


def _legend(ax, bg: str, theme: Theme) -> None:
    leg = ax.legend(
        loc="upper left",
        fontsize=9,
        framealpha=0.85,
        edgecolor="none",
        facecolor=bg,
    )
    for text in leg.get_texts():
        text.set_color(theme.legend_text)


def _figure_name(fig: plt.Figure) -> str:
    name = fig.get_label().strip() or "figure"
    slug = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()
    return slug or "figure"


def _save_all_figures(
    figures: list[plt.Figure],
    output_root: str,
    output_dir: str | None = None,
) -> str:
    if output_dir is None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_dir = os.path.abspath(os.path.join(output_root, timestamp))
    os.makedirs(output_dir, exist_ok=True)
    for fig in figures:
        fig.canvas.draw()
        fig.savefig(os.path.join(output_dir, f"{_figure_name(fig)}.png"), dpi=180)
    return output_dir


def _apply_theme(theme: Theme) -> None:
    plt.style.use("dark_background")
    plt.rcParams.update(
        {
            "font.size": 10,
            "axes.titlesize": 13,
            "axes.titleweight": "semibold",
            "axes.labelsize": 10,
            "figure.facecolor": theme.fig_bg,
            "axes.facecolor": theme.ax_bg,
            "text.color": theme.text,
            "axes.labelcolor": theme.text,
            "xtick.color": theme.tick,
            "ytick.color": theme.tick,
        }
    )


def _create_timing_panel(theme: Theme) -> TimingPanel:
    fig = plt.figure("Performance Timing", figsize=(8, 5))
    fig.set_facecolor(theme.fig_bg)
    ax = fig.add_subplot(111)
    _style_axis(ax, theme.ax_bg, theme)
    (line_inf,) = ax.plot(
        [], [], label="Inference", color=theme.infer, linewidth=theme.linewidth
    )
    (line_period,) = ax.plot(
        [], [], label="NN Period", color=theme.period, linewidth=theme.linewidth
    )
    ax.set_title("Performance Timing", color=theme.title, pad=10)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Microseconds")
    _legend(ax, theme.ax_bg, theme)
    fig.tight_layout()
    return TimingPanel(fig=fig, ax=ax, line_inf=line_inf, line_period=line_period)


def _create_cpu_panel(theme: Theme) -> CpuPanel:
    fig = plt.figure("CPU Utilisation", figsize=(6, 5))
    fig.set_facecolor(theme.fig_bg)
    ax = fig.add_subplot(111)
    _style_axis(ax, theme.ax_bg, theme)
    (line_cpu,) = ax.plot(
        [], [], label="CPU %", color=theme.cpu, linewidth=theme.linewidth
    )
    ax.set_ylim(auto=True)
    ax.set_title("CPU Utilisation", color=theme.title, pad=10)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("%")
    cpu_percent_text = ax.text(
        0.98,
        0.95,
        "0%",
        transform=ax.transAxes,
        ha="right",
        va="top",
        fontsize=15,
        fontweight="bold",
        color=theme.text,
    )
    cpu_freq_text = ax.text(
        0.98,
        0.05,
        "",
        transform=ax.transAxes,
        ha="right",
        va="bottom",
        fontsize=9,
        color=theme.muted,
    )
    fig.tight_layout()
    return CpuPanel(
        fig=fig,
        ax=ax,
        line_cpu=line_cpu,
        cpu_percent_text=cpu_percent_text,
        cpu_freq_text=cpu_freq_text,
    )


def _create_tof_heatmap(fig: plt.Figure, ax, title: str, cmap: str, vmin: float, vmax: float, cbar_label: str, theme: Theme):
    ax.set_facecolor(theme.ax_bg)
    img = ax.imshow(
        np.full((8, 8), np.nan),
        cmap=cmap,
        interpolation="nearest",
        vmin=vmin,
        vmax=vmax,
    )
    cbar = fig.colorbar(img, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label(cbar_label, rotation=270, labelpad=15, color=theme.text)
    cbar.ax.yaxis.set_tick_params(color=theme.tick)
    for label in cbar.ax.get_yticklabels():
        label.set_color(theme.tick)
    ax.set_title(title, color=theme.title, pad=10)
    ax.set_xticks(np.arange(-0.5, 8, 1), minor=True)
    ax.set_yticks(np.arange(-0.5, 8, 1), minor=True)
    ax.grid(which="minor", color="w", linestyle="-", linewidth=0.5, alpha=0.15)
    ax.tick_params(which="minor", bottom=False, left=False)
    ax.set_xticks([])
    ax.set_yticks([])
    return img


def _create_tof_panel(theme: Theme) -> TofPanel:
    fig = plt.figure("ToF Grids", figsize=(12, 4.5))
    fig.set_facecolor(theme.fig_bg)
    fig.subplots_adjust(left=0.025, right=0.94, top=0.90, bottom=0.06)
    grid = fig.add_gridspec(1, 3, wspace=0.22)
    ax_depth = fig.add_subplot(grid[0, 0])
    ax_sigma = fig.add_subplot(grid[0, 1])
    ax_signal = fig.add_subplot(grid[0, 2])
    return TofPanel(
        fig=fig,
        img_depth=_create_tof_heatmap(
            fig, ax_depth, "Distance", "magma", 0, 2000, "Distance (mm)", theme
        ),
        img_sigma=_create_tof_heatmap(
            fig, ax_sigma, "Range Sigma", "viridis", 0, 80, "Sigma (mm)", theme
        ),
        img_signal=_create_tof_heatmap(
            fig,
            ax_signal,
            "Signal / SPAD",
            "cividis",
            0,
            300,
            "kcps / spad",
            theme,
        ),
    )


def _create_power_panel(theme: Theme) -> PowerPanel:
    fig = plt.figure("Power Consumption", figsize=(8, 5))
    fig.set_facecolor(theme.fig_bg)
    ax = fig.add_subplot(111)
    _style_axis(ax, theme.ax_bg, theme)
    (line_period,) = ax.plot(
        [],
        [],
        label="Period",
        color=theme.power_period,
        linewidth=theme.linewidth,
        linestyle="--",
        zorder=3,
    )
    ax.set_title("Power Consumption", color=theme.title, pad=10)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("mW")
    peak_text = ax.text(
        0.98,
        0.95,
        "",
        transform=ax.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color=theme.accent,
    )
    _legend(ax, theme.ax_bg, theme)
    fig.tight_layout()
    return PowerPanel(fig=fig, ax=ax, line_period=line_period, peak_text=peak_text)


def _create_energy_panel(theme: Theme) -> EnergyPanel:
    fig = plt.figure("Power Energy (mJ)", figsize=(8, 5))
    fig.set_facecolor(theme.fig_bg)
    ax = fig.add_subplot(111)
    _style_axis(ax, theme.ax_bg, theme)
    (line_period_mj,) = ax.plot(
        [],
        [],
        label="Period",
        color=theme.power_mj,
        linewidth=theme.linewidth,
        linestyle="--",
        zorder=3,
    )
    ax.set_title("Power Energy", color=theme.title, pad=10)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("mJ")
    stats_text = ax.text(
        0.98,
        0.95,
        "",
        transform=ax.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color=theme.power_mj,
    )
    _legend(ax, theme.ax_bg, theme)
    fig.tight_layout()
    return EnergyPanel(fig=fig, ax=ax, line_period_mj=line_period_mj, stats_text=stats_text)


def _create_battery_panel(state, theme: Theme) -> BatteryPanel:
    fig = plt.figure("Battery Life Estimate", figsize=(8, 5))
    fig.set_facecolor(theme.fig_bg)
    fig.subplots_adjust(bottom=0.18)
    ax = fig.add_subplot(111)
    _style_axis(ax, theme.ax_bg, theme)
    (line_hours,) = ax.plot(
        [], [], label="Est. runtime", color=theme.battery, linewidth=theme.linewidth
    )
    ax.set_title("Battery Life Estimate", color=theme.title, pad=10)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Hours")
    ax.text(
        0.02,
        0.02,
        "mWh = mAh × V_supply; hours = mWh / P_avg.  P_avg uses period energy over the paired monitor-window duration.",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=8,
        color=theme.muted,
    )
    stats_text = ax.text(
        0.98,
        0.98,
        "",
        transform=ax.transAxes,
        ha="right",
        va="top",
        fontsize=10,
        color=theme.title,
        linespacing=1.2,
        bbox={
            "boxstyle": "round,pad=0.35",
            "facecolor": theme.panel_box,
            "edgecolor": theme.spine,
            "alpha": 0.92,
        },
    )
    _legend(ax, theme.ax_bg, theme)

    ax_tb = fig.add_axes([0.12, 0.03, 0.32, 0.065])
    battery_box = TextBox(
        ax_tb,
        "mAh ",
        initial=f"{state.battery_capacity_mah:g}",
        color=theme.widget_bg,
        hovercolor=theme.widget_hover,
    )
    battery_box.text_disp.set_color(theme.text)
    battery_box.label.set_color(theme.text)
    return BatteryPanel(
        fig=fig,
        ax=ax,
        line_hours=line_hours,
        stats_text=stats_text,
        battery_box=battery_box,
    )


def _create_info_panel(theme: Theme) -> InfoPanel:
    fig = plt.figure("Device Info", figsize=(8, 7))
    fig.set_facecolor(theme.fig_bg)
    grid = fig.add_gridspec(2, 1, height_ratios=[9, 0.75])
    ax_text = fig.add_subplot(grid[0])
    ax_text.axis("off")
    ax_text.set_facecolor(theme.fig_bg)
    text_box = ax_text.text(
        0.03,
        0.99,
        "",
        transform=ax_text.transAxes,
        va="top",
        ha="left",
        family="monospace",
        fontsize=8.5,
        color=theme.text,
        linespacing=1.35,
        clip_on=False,
    )

    ax_btn = fig.add_subplot(grid[1])
    ax_btn.axis("off")
    btn_ax1 = fig.add_axes([0.04, 0.035, 0.21, 0.05])
    btn_ax2 = fig.add_axes([0.28, 0.035, 0.21, 0.05])
    btn_ax3 = fig.add_axes([0.52, 0.035, 0.21, 0.05])
    btn_ax4 = fig.add_axes([0.76, 0.035, 0.20, 0.05])
    btn_info = Button(btn_ax1, "Get Device Info", color=theme.widget_bg, hovercolor=theme.widget_hover)
    btn_toggle = Button(btn_ax2, "Toggle Display", color=theme.widget_bg, hovercolor=theme.widget_hover)
    btn_record = Button(btn_ax3, "Start Recording", color=theme.record_off, hovercolor=theme.record_off_hover)
    btn_save_all = Button(btn_ax4, "Save Figures", color=theme.widget_bg, hovercolor=theme.widget_hover)
    for button in (btn_info, btn_toggle, btn_record, btn_save_all):
        button.label.set_color(theme.text)
        button.label.set_fontsize(10)

    status_text = ax_btn.text(
        0.99,
        0.98,
        "",
        va="top",
        ha="right",
        fontsize=8.5,
        color=theme.muted,
        transform=ax_btn.transAxes,
    )
    fig.tight_layout()
    fig.subplots_adjust(left=0.05, right=0.98, top=0.98)
    return InfoPanel(
        fig=fig,
        text_box=text_box,
        status_text=status_text,
        btn_info=btn_info,
        btn_toggle=btn_toggle,
        btn_record=btn_record,
        btn_save_all=btn_save_all,
    )


def _create_config_panel(state, theme: Theme) -> ConfigPanel:
    fig = plt.figure("Runtime App Config", figsize=(6.5, 6.4))
    fig.set_facecolor(theme.fig_bg)
    fig.subplots_adjust(left=0.05, right=0.95, top=0.97, bottom=0.03)
    ax = fig.add_subplot(111)
    ax.axis("off")
    title = ax.text(
        0.04,
        0.97,
        "Runtime App Config",
        va="top",
        ha="left",
        fontsize=12,
        color=theme.title,
        fontweight="semibold",
        clip_on=False,
    )
    status_text = ax.text(
        0.04,
        0.92,
        "",
        va="top",
        ha="left",
        fontsize=8.5,
        color=theme.muted,
        clip_on=False,
    )

    labels = [
        ("PP conf threshold", "pp_conf_threshold"),
        ("PP IoU threshold", "pp_iou_threshold"),
        ("Track thresh", "track_thresh"),
        ("Det thresh", "det_thresh"),
        ("Sim1 thresh", "sim1_thresh"),
        ("Sim2 thresh", "sim2_thresh"),
        ("Tlost cnt", "tlost_cnt"),
        ("Alert threshold mm", "alert_threshold_mm"),
    ]
    textboxes: dict[str, TextBox] = {}
    y0 = 0.78
    dy = 0.09
    for idx, (label, key) in enumerate(labels):
        y = y0 - idx * dy
        ax_tb = fig.add_axes([0.38, y, 0.22, 0.055])
        initial = (
            f"{getattr(state, key):.4f}"
            if key not in ("tlost_cnt", "alert_threshold_mm")
            else f"{int(getattr(state, key))}"
        )
        tb = TextBox(
            ax_tb,
            f"{label}: ",
            initial=initial,
            color=theme.widget_bg,
            hovercolor=theme.widget_hover,
        )
        tb.text_disp.set_color(theme.text)
        tb.label.set_color(theme.text)
        textboxes[key] = tb

    btn_load = Button(
        fig.add_axes([0.10, 0.08, 0.25, 0.07]),
        "Load Device",
        color=theme.widget_bg,
        hovercolor=theme.widget_hover,
    )
    btn_apply = Button(
        fig.add_axes([0.62, 0.08, 0.25, 0.07]),
        "Apply Config",
        color=theme.widget_bg,
        hovercolor=theme.widget_hover,
    )
    btn_load.label.set_color(theme.text)
    btn_apply.label.set_color(theme.text)
    fig._visualizer_widgets = (title, status_text, btn_load, btn_apply, *textboxes.values())
    return ConfigPanel(
        fig=fig,
        status_text=status_text,
        textboxes=textboxes,
        btn_load=btn_load,
        btn_apply=btn_apply,
    )


def _create_bbox_panel(theme: Theme) -> BboxPanel:
    fig = plt.figure("Bounding Boxes", figsize=(7, 7))
    fig.set_facecolor(theme.fig_bg)
    fig.subplots_adjust(left=0.14, right=0.96, top=0.90, bottom=0.10)
    return BboxPanel(fig=fig, ax=fig.add_subplot(111))


def _refresh_record_button(info_panel: InfoPanel, recorder, theme: Theme) -> None:
    if recorder.is_recording():
        info_panel.btn_record.label.set_text("Stop Recording")
        info_panel.btn_record.color = theme.record_on
        info_panel.btn_record.hovercolor = theme.record_on_hover
        info_panel.btn_record.ax.set_facecolor(theme.record_on)
    else:
        info_panel.btn_record.label.set_text("Start Recording")
        info_panel.btn_record.color = theme.record_off
        info_panel.btn_record.hovercolor = theme.record_off_hover
        info_panel.btn_record.ax.set_facecolor(theme.record_off)


def _populate_cfg_boxes(state, config_panel: ConfigPanel) -> None:
    config_panel.textboxes["pp_conf_threshold"].set_val(f"{state.pp_conf_threshold:.4f}")
    config_panel.textboxes["pp_iou_threshold"].set_val(f"{state.pp_iou_threshold:.4f}")
    config_panel.textboxes["track_thresh"].set_val(f"{state.track_thresh:.4f}")
    config_panel.textboxes["det_thresh"].set_val(f"{state.det_thresh:.4f}")
    config_panel.textboxes["sim1_thresh"].set_val(f"{state.sim1_thresh:.4f}")
    config_panel.textboxes["sim2_thresh"].set_val(f"{state.sim2_thresh:.4f}")
    config_panel.textboxes["tlost_cnt"].set_val(f"{int(state.tlost_cnt)}")
    config_panel.textboxes["alert_threshold_mm"].set_val(f"{int(state.alert_threshold_mm)}")


def _read_cfg_boxes(config_panel: ConfigPanel) -> dict[str, float | int]:
    return {
        "pp_conf_threshold": float(config_panel.textboxes["pp_conf_threshold"].text.strip()),
        "pp_iou_threshold": float(config_panel.textboxes["pp_iou_threshold"].text.strip()),
        "track_thresh": float(config_panel.textboxes["track_thresh"].text.strip()),
        "det_thresh": float(config_panel.textboxes["det_thresh"].text.strip()),
        "sim1_thresh": float(config_panel.textboxes["sim1_thresh"].text.strip()),
        "sim2_thresh": float(config_panel.textboxes["sim2_thresh"].text.strip()),
        "tlost_cnt": int(config_panel.textboxes["tlost_cnt"].text.strip()),
        "alert_threshold_mm": int(
            config_panel.textboxes["alert_threshold_mm"].text.strip()
        ),
    }


def _wire_battery_controls(state, battery_panel: BatteryPanel) -> None:
    def on_battery_mah(text: str) -> None:
        try:
            value = float(str(text).strip().replace(",", ""))
            if value > 0.0:
                state.battery_capacity_mah = value
        except ValueError:
            pass

    battery_panel.battery_box.on_submit(on_battery_mah)


def _wire_info_controls(
    state,
    cmd_queue: Queue[tuple[str, Any]],
    recorder,
    figures: list[plt.Figure],
    info_panel: InfoPanel,
    theme: Theme,
) -> None:
    def on_get_info(_event) -> None:
        cmd_queue.put(("get_info", None))

    def on_toggle_display(_event) -> None:
        cmd_queue.put(("set_display", not state.display_enabled))

    def on_save_all(_event) -> None:
        try:
            output_dir = _save_all_figures(figures, recorder.output_root)
            info_panel.status_text.set_text(f"Saved screenshots to {output_dir}")
            print(f"[gui] Saved screenshots to {output_dir}")
        except Exception as exc:
            info_panel.status_text.set_text(f"Screenshot save failed: {exc}")
            print(f"[gui] Screenshot save failed: {exc}")
        info_panel.fig.canvas.draw_idle()

    def on_record(_event) -> None:
        try:
            if recorder.is_recording():
                output_dir = recorder.stop(state)
                _save_all_figures(figures, recorder.output_root, output_dir=output_dir)
                info_panel.status_text.set_text(f"Saved recording to {output_dir}")
                print(f"[gui] Saved recording bundle to {output_dir}")
            else:
                output_dir = recorder.start(state)
                info_panel.status_text.set_text(f"Recording to {output_dir}")
                print(f"[gui] Recording started: {output_dir}")
        except Exception as exc:
            info_panel.status_text.set_text(f"Recording failed: {exc}")
            print(f"[gui] Recording failed: {exc}")
        _refresh_record_button(info_panel, recorder, theme)
        info_panel.fig.canvas.draw_idle()

    info_panel.btn_info.on_clicked(on_get_info)
    info_panel.btn_toggle.on_clicked(on_toggle_display)
    info_panel.btn_record.on_clicked(on_record)
    info_panel.btn_save_all.on_clicked(on_save_all)
    _refresh_record_button(info_panel, recorder, theme)
    info_panel.fig._visualizer_widgets = (
        info_panel.btn_info,
        info_panel.btn_toggle,
        info_panel.btn_record,
        info_panel.btn_save_all,
        info_panel.status_text,
    )


def _wire_config_controls(
    state,
    cmd_queue: Queue[tuple[str, Any]],
    config_panel: ConfigPanel,
) -> None:
    def on_load_cfg(_event) -> None:
        _populate_cfg_boxes(state, config_panel)
        state.pp_cfg_status_text = "Loaded runtime values from latest DeviceInfo."
        state.pp_cfg_pending_device_info = 0
        config_panel.status_text.set_text(state.pp_cfg_status_text)

    def on_apply_cfg(_event) -> None:
        try:
            cfg = _read_cfg_boxes(config_panel)
            state.pp_cfg_status_text = (
                "Sent set_app_config. Waiting for ACK/DeviceInfo..."
            )
            state.pp_cfg_pending_device_info += 1
            config_panel.status_text.set_text(state.pp_cfg_status_text)
            cmd_queue.put(("set_app_config", cfg))
            cmd_queue.put(("get_info", None))
        except ValueError:
            state.pp_cfg_status_text = (
                "Invalid number format. Please enter valid numeric values."
            )
            state.pp_cfg_pending_device_info = 0
            config_panel.status_text.set_text(state.pp_cfg_status_text)

    config_panel.btn_load.on_clicked(on_load_cfg)
    config_panel.btn_apply.on_clicked(on_apply_cfg)
    _populate_cfg_boxes(state, config_panel)


def _update_timing(_frame_idx, state, panel: TimingPanel):
    x = _series_secs(state, state.timing_time_hist)
    panel.line_inf.set_data(x, np.array(state.infer_hist))
    panel.line_period.set_data(x, np.array(state.period_hist))
    panel.ax.relim()
    panel.ax.autoscale_view()
    return (panel.line_inf, panel.line_period)


def _update_cpu(_frame_idx, state, panel: CpuPanel):
    pct = state.cpu_usage_percent
    mcu_mhz = int(state.mcu_freq_mhz)
    cpu_usage_mhz = (pct / 100.0) * float(mcu_mhz) if mcu_mhz > 0 else 0.0
    x_cpu = _series_secs(state, state.cpu_time_hist)
    panel.line_cpu.set_data(x_cpu, np.array(state.cpu_hist))
    panel.ax.relim()
    panel.ax.autoscale_view()
    panel.cpu_percent_text.set_text(f"{pct:.1f}%")
    npu = int(state.npu_freq_mhz)
    if mcu_mhz > 0:
        panel.cpu_freq_text.set_text(
            f"{cpu_usage_mhz:.0f} MHz  |  CPU {mcu_mhz} MHz / NPU {npu} MHz"
        )
    else:
        panel.cpu_freq_text.set_text(f"CPU {mcu_mhz} MHz / NPU {npu} MHz")
    return (panel.line_cpu, panel.cpu_percent_text, panel.cpu_freq_text)


def _update_tof(_frame_idx, state, panel: TofPanel):
    panel.img_depth.set_data(state.tof_depth_grid)
    panel.img_sigma.set_data(state.tof_sigma_grid)
    panel.img_signal.set_data(state.tof_signal_grid)
    return (panel.img_depth, panel.img_sigma, panel.img_signal)


def _update_power(_frame_idx, state, panel: PowerPanel):
    if state.battery_time_hist and state.battery_p_avg_mw_hist:
        x_period = _series_secs(state, state.battery_time_hist)
        y_period = np.array(state.battery_p_avg_mw_hist, dtype=float)
    else:
        x_period = np.array([])
        y_period = np.array([])
    panel.line_period.set_data(x_period, y_period)

    if state.battery_p_avg_mw_hist:
        panel.peak_text.set_text(f"period: {float(state.battery_p_avg_mw_hist[-1]):.0f} mW")
    else:
        panel.peak_text.set_text("")
    panel.ax.relim()
    panel.ax.autoscale_view()
    return (panel.line_period, panel.peak_text)


def _update_energy(_frame_idx, state, panel: EnergyPanel):
    if state.pm_period_mj_time_hist:
        x_period = _series_secs(state, state.pm_period_mj_time_hist)
        y_period = np.array(state.pm_period_mj_hist, dtype=float)
    else:
        x_period = np.array([])
        y_period = np.array([])
    panel.line_period_mj.set_data(x_period, y_period)
    if state.pm_period_total_mj > 0.0:
        panel.stats_text.set_text(f"period: {state.pm_period_total_mj:.2f} mJ")
    else:
        panel.stats_text.set_text("")
    panel.ax.relim()
    panel.ax.autoscale_view()
    return (panel.line_period_mj, panel.stats_text)


def _update_battery(_frame_idx, state, panel: BatteryPanel):
    energy_mwh = float(state.battery_capacity_mah) * float(state.battery_supply_voltage_v)
    if state.battery_time_hist and state.battery_p_avg_mw_hist:
        x = _series_secs(state, state.battery_time_hist)
        power_mw = np.array(state.battery_p_avg_mw_hist, dtype=float)
        hours = energy_mwh / power_mw
        panel.line_hours.set_data(x, hours)
        panel.stats_text.set_text(
            f"{state.battery_capacity_mah:g} mAh × {state.battery_supply_voltage_v:g} V → {energy_mwh:g} mWh\n"
            f"÷ {float(power_mw[-1]):.0f} mW ≈ {float(hours[-1]):.2f} h"
        )
    else:
        panel.line_hours.set_data([], [])
        panel.stats_text.set_text("—\n(need power link + frames)")
    panel.ax.relim()
    panel.ax.autoscale_view()
    return (panel.line_hours, panel.stats_text)


def _update_bbox(_frame_idx, state, panel: BboxPanel, theme: Theme):
    ax = panel.ax
    ax.clear()
    _style_axis(ax, theme.ax_bg, theme)
    ax.set_xlim(0.0, 1.0)
    ax.set_ylim(1.0, 0.0)
    ax.set_aspect("equal")
    ax.set_title("Bounding Boxes", color=theme.title, pad=12)
    ax.set_xlabel("x (0 = left)")
    ax.set_ylabel("y (0 = top)")
    ax.yaxis.labelpad = 8
    ax.text(
        0.04,
        0.04,
        f"frame t = {int(state.detection_timestamp)} ms   det: {state.detection_count}   trk: {state.tracked_box_count}",
        transform=ax.transAxes,
        ha="left",
        va="bottom",
        fontsize=9,
        color=theme.muted,
        clip_on=False,
    )
    labels = state.class_labels
    for det in state.detection_boxes:
        x0 = det.cx - 0.5 * det.w
        y0 = det.cy - 0.5 * det.h
        ax.add_patch(
            Rectangle(
                (x0, y0),
                det.w,
                det.h,
                linewidth=1.8,
                edgecolor=theme.infer,
                facecolor=theme.infer,
                alpha=0.14,
            )
        )
        class_name = labels[det.class_id] if 0 <= det.class_id < len(labels) else str(det.class_id)
        ax.text(
            x0,
            y0 - 0.02,
            f"{class_name} {det.score:.2f}",
            ha="left",
            va="bottom",
            fontsize=8,
            color=theme.infer,
            clip_on=False,
        )
    for track in state.track_boxes:
        x0 = track.cx - 0.5 * track.w
        y0 = track.cy - 0.5 * track.h
        distance_mm = state.track_person_mm.get(track.track_id)
        track_face = theme.track_box
        track_alpha = 0.12
        if distance_mm is not None:
            if not state.tof_stale and distance_mm <= state.alert_threshold_mm:
                track_face = "#C00000"
                track_alpha = 0.18
            elif not state.tof_stale:
                track_face = theme.accent
                track_alpha = 0.16
        ax.add_patch(
            Rectangle(
                (x0, y0),
                track.w,
                track.h,
                linewidth=2.0,
                edgecolor=theme.track_box,
                facecolor=track_face,
                alpha=track_alpha,
            )
        )
        if distance_mm is not None:
            badge_h = min(track.h, max(0.026, min(0.045, track.h * 0.22)))
            badge_color = theme.muted if state.tof_stale else track_face
            badge_alpha = 0.24 if state.tof_stale else 0.34
            ax.add_patch(
                Rectangle(
                    (x0, y0),
                    track.w,
                    badge_h,
                    linewidth=0.0,
                    facecolor=badge_color,
                    alpha=badge_alpha,
                )
            )
            ax.text(
                x0 + 0.006,
                y0 + 0.5 * badge_h,
                f"ToF {distance_mm} mm",
                ha="left",
                va="center",
                fontsize=8,
                color=theme.title,
                fontweight="bold",
                clip_on=True,
            )
        ax.text(
            track.cx,
            y0 - 0.02,
            f"id {track.track_id}",
            ha="center",
            va="bottom",
            fontsize=8,
            color=theme.track_box,
            clip_on=False,
        )
    if not state.detection_boxes and not state.track_boxes:
        ax.text(
            0.5,
            0.5,
            "No boxes in last frame",
            ha="center",
            va="center",
            fontsize=11,
            color=theme.muted,
            transform=ax.transAxes,
        )
    legend = ax.legend(
        handles=[
            Line2D([0], [0], color=theme.infer, lw=2.4, label="Detections (PP)"),
            Line2D([0], [0], color=theme.track_box, lw=2.4, label="Tracks"),
        ],
        loc="upper right",
        fontsize=9,
        framealpha=0.85,
        edgecolor="none",
        facecolor=theme.ax_bg,
    )
    for text in legend.get_texts():
        text.set_color(theme.legend_text)
    return ()


def _build_tof_distance_summary(state) -> str:
    if not state.track_person_mm:
        return "--"
    return ", ".join(
        f"id {track_id}: {distance_mm} mm"
        for track_id, distance_mm in sorted(state.track_person_mm.items())
    )


def _build_info_lines(state) -> str:
    sep = "\u2500" * 30
    status = "ALERT" if state.tof_alert else "OK"
    stale = "stale" if state.tof_stale else "fresh"
    class_labels = ", ".join(state.class_labels) or "-"
    recog_host_fw = "yes" if state.host_introduced else "no"
    recog_fw_host = "yes" if state.firmware_recognized else "no"
    fw_path = state.firmware_port_path or "-"
    fw_baud = f"{state.firmware_baud}" if state.firmware_baud else "-"
    last_pp = state.post_hist[-1] if state.post_hist else 0.0
    last_trk = state.tracker_hist[-1] if state.tracker_hist else 0.0
    tof_distances = _build_tof_distance_summary(state)
    return "\n".join(
        [
            " Device",
            sep,
            f"  Model   {state.model_name}   NN: {state.nn_size_text}",
            f"  Display {state.display_width}x{state.display_height}   Letterbox: {state.letterbox_width}x{state.letterbox_height}",
            f"  Camera  {state.camera_mode_text}   Mode: {state.build_mode_text}",
            f"  Built   {state.build_timestamp}",
            "",
            " Connection",
            sep,
            f"  STM32   port {fw_path} @ {fw_baud} bps",
            f"          serial open: {'yes' if state.firmware_connected else 'no'}   H→D: {recog_host_fw}   D→H: {recog_fw_host}  (need yes/yes for traffic)",
            f"  Display {'on' if state.display_enabled else 'off'}",
            "",
            " Sensors",
            sep,
            f"  ToF     persons {tof_distances}   {status} ({stale})",
            f"          alert threshold {state.alert_threshold_mm} mm",
            f"  ESP32   {'connected' if state.power_connected else 'disconnected'}",
            "",
            " Stats",
            sep,
            f"  Frames  {state.frame_count}   Drops: {state.frame_drops}",
            f"  Timing  PP: {last_pp:.0f} us   Trk: {last_trk:.0f} us   Idle: {state.nn_idle_us} us",
            f"  Timestamp  msg: {state.last_timestamp} ms   detection: {state.detection_timestamp} ms   info: {state.device_info_timestamp} ms   ack: {state.last_ack_timestamp} ms",
            f"  Labels  {class_labels}",
            f"  ACK     {state.last_ack}",
            f"  Err     {state.last_error or '-'}",
            f"  PwrErr  {state.last_power_error or '-'}",
        ]
    )


def _update_info(_frame_idx, state, panel: InfoPanel):
    panel.text_box.set_text(_build_info_lines(state))
    return (panel.text_box,)


def _update_config(_frame_idx, state, panel: ConfigPanel):
    panel.status_text.set_text(state.pp_cfg_status_text)
    return (panel.status_text,)


def _connect_close_handlers(figures: list[plt.Figure]) -> None:
    closing_all = [False]

    def on_any_figure_close(_event) -> None:
        if closing_all[0]:
            return
        closing_all[0] = True
        plt.close("all")

    for fig in figures:
        fig.canvas.mpl_connect("close_event", on_any_figure_close)


def create_gui(
    state,
    cmd_queue: Queue[tuple[str, Any]],
    recorder,
) -> tuple[list[plt.Figure], list[FuncAnimation]]:
    theme = Theme()
    _apply_theme(theme)

    timing_panel = _create_timing_panel(theme)
    cpu_panel = _create_cpu_panel(theme)
    tof_panel = _create_tof_panel(theme)
    power_panel = _create_power_panel(theme)
    energy_panel = _create_energy_panel(theme)
    battery_panel = _create_battery_panel(state, theme)
    bbox_panel = _create_bbox_panel(theme)
    info_panel = _create_info_panel(theme)
    config_panel = _create_config_panel(state, theme)

    figures = [
        timing_panel.fig,
        cpu_panel.fig,
        tof_panel.fig,
        power_panel.fig,
        energy_panel.fig,
        battery_panel.fig,
        bbox_panel.fig,
        info_panel.fig,
        config_panel.fig,
    ]

    _wire_battery_controls(state, battery_panel)
    _wire_info_controls(state, cmd_queue, recorder, figures, info_panel, theme)
    _wire_config_controls(state, cmd_queue, config_panel)

    interval = 33

    animations = [
        FuncAnimation(
            timing_panel.fig,
            lambda frame_idx: _update_timing(frame_idx, state, timing_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            cpu_panel.fig,
            lambda frame_idx: _update_cpu(frame_idx, state, cpu_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            tof_panel.fig,
            lambda frame_idx: _update_tof(frame_idx, state, tof_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            power_panel.fig,
            lambda frame_idx: _update_power(frame_idx, state, power_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            energy_panel.fig,
            lambda frame_idx: _update_energy(frame_idx, state, energy_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            battery_panel.fig,
            lambda frame_idx: _update_battery(frame_idx, state, battery_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            bbox_panel.fig,
            lambda frame_idx: _update_bbox(frame_idx, state, bbox_panel, theme),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            info_panel.fig,
            lambda frame_idx: _update_info(frame_idx, state, info_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
        FuncAnimation(
            config_panel.fig,
            lambda frame_idx: _update_config(frame_idx, state, config_panel),
            interval=interval,
            blit=False,
            cache_frame_data=False,
        ),
    ]

    _connect_close_handlers(figures)
    return figures, animations
