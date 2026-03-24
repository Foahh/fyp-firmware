#!/usr/bin/env python3

import io
import statistics
from collections import Counter
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


def _event_thread_name(event):
    for attr in ("thread_name", "thread", "thread_ptr_name"):
        value = getattr(event, attr, None)
        if value:
            return str(value)
    # Fallback to generic text.
    text = str(event)
    if ":" in text:
        return text.split(":", 1)[0].strip()
    return "<unknown>"


def _event_name(event):
    for attr in ("fn_name", "name", "event_name"):
        value = getattr(event, attr, None)
        if value:
            return str(value)
    text = str(event)
    if ":" in text:
        text = text.split(":", 1)[1]
    if "(" in text:
        text = text.split("(", 1)[0]
    return text.strip() or "<unknown>"


def _event_timestamp(event):
    for attr in ("timestamp", "time", "event_time", "ticks"):
        value = getattr(event, attr, None)
        if value is None:
            continue
        try:
            return int(value)
        except (TypeError, ValueError):
            continue
    return None


def _context_switch_pairs(events):
    pairs = Counter()
    prev = None
    for ev in events:
        cur = _event_thread_name(ev)
        if (
            prev is not None
            and cur != prev
            and cur != "<unknown>"
            and prev != "<unknown>"
            and cur != "INTERRUPT"
            and prev != "INTERRUPT"
        ):
            pairs[(prev, cur)] += 1
        prev = cur
    return pairs


def _thread_runtime_slices(events):
    slices = Counter()
    per_thread = {}
    interrupt_ticks = 0
    unknown_ticks = 0
    total_ticks = 0

    prev_ts = None
    prev_thread = None
    for ev in events:
        cur_ts = _event_timestamp(ev)
        cur_thread = _event_thread_name(ev)
        if prev_ts is not None and cur_ts is not None:
            dt = cur_ts - prev_ts
            if dt < 0:
                dt = 0
            total_ticks += dt
            if prev_thread == "INTERRUPT":
                interrupt_ticks += dt
            elif prev_thread in (None, "<unknown>"):
                unknown_ticks += dt
            else:
                slices[prev_thread] += dt
                per_thread.setdefault(prev_thread, []).append(dt)
        prev_ts = cur_ts
        prev_thread = cur_thread

    return {
        "ticks_per_thread": slices,
        "slice_samples": per_thread,
        "interrupt_ticks": interrupt_ticks,
        "unknown_ticks": unknown_ticks,
        "total_ticks": total_ticks,
    }


def _quantile(sorted_values, q):
    if not sorted_values:
        return 0
    if len(sorted_values) == 1:
        return sorted_values[0]
    pos = (len(sorted_values) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(sorted_values) - 1)
    frac = pos - lo
    return int(sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * frac)


def _slice_stats(samples_by_thread):
    stats = []
    for thread, samples in samples_by_thread.items():
        if not samples:
            continue
        ordered = sorted(samples)
        stats.append(
            {
                "thread": thread,
                "samples": len(ordered),
                "slice_min_ticks": ordered[0],
                "slice_p50_ticks": _quantile(ordered, 0.50),
                "slice_p95_ticks": _quantile(ordered, 0.95),
                "slice_max_ticks": ordered[-1],
                "slice_mean_ticks": int(statistics.fmean(ordered)),
            }
        )
    stats.sort(key=lambda x: x["slice_p95_ticks"], reverse=True)
    return stats


def _wait_balance(events):
    by_thread = {}
    for ev in events:
        thread = _event_thread_name(ev)
        name = _event_name(ev)
        rec = by_thread.setdefault(
            thread,
            {
                "sem_get": 0,
                "sem_put": 0,
                "flags_get": 0,
                "flags_set": 0,
                "mtx_get": 0,
                "mtx_put": 0,
                "sleep": 0,
            },
        )
        if name == "semGet":
            rec["sem_get"] += 1
        elif name in ("semPut", "semCeilPut"):
            rec["sem_put"] += 1
        elif name == "flagsGet":
            rec["flags_get"] += 1
        elif name == "flagsSet":
            rec["flags_set"] += 1
        elif name == "mtxGet":
            rec["mtx_get"] += 1
        elif name == "mtxPut":
            rec["mtx_put"] += 1
        elif name == "threadSleep":
            rec["sleep"] += 1

    rows = []
    for thread, rec in by_thread.items():
        if thread in ("INTERRUPT", "<unknown>"):
            continue
        rows.append(
            {
                "thread": thread,
                **rec,
                "sem_wait_pressure": rec["sem_get"] - rec["sem_put"],
                "flags_wait_pressure": rec["flags_get"] - rec["flags_set"],
                "mtx_wait_pressure": rec["mtx_get"] - rec["mtx_put"],
            }
        )
    rows.sort(
        key=lambda x: (
            abs(x["sem_wait_pressure"]) + abs(x["flags_wait_pressure"]) + abs(x["mtx_wait_pressure"]),
            x["sleep"],
        ),
        reverse=True,
    )
    return rows


def _interrupt_burst_stats(events):
    burst_ticks = []
    burst_events = []
    burst_start_ts = None
    burst_last_ts = None
    burst_count = 0
    in_burst = False
    prev_ts = None
    prev_thread = None

    for ev in events:
        cur_ts = _event_timestamp(ev)
        cur_thread = _event_thread_name(ev)
        if prev_ts is not None and cur_ts is not None:
            dt = cur_ts - prev_ts
            if dt < 0:
                dt = 0
            if prev_thread == "INTERRUPT":
                if not in_burst:
                    in_burst = True
                    burst_start_ts = prev_ts
                    burst_last_ts = cur_ts
                    burst_count = 1
                else:
                    burst_last_ts = cur_ts
                    burst_count += 1
            elif in_burst:
                burst_ticks.append(max(0, burst_last_ts - burst_start_ts))
                burst_events.append(burst_count)
                in_burst = False

        prev_ts = cur_ts
        prev_thread = cur_thread

    if in_burst and burst_start_ts is not None and burst_last_ts is not None:
        burst_ticks.append(max(0, burst_last_ts - burst_start_ts))
        burst_events.append(burst_count)

    if not burst_ticks:
        return {
            "count": 0,
            "p50_ticks": 0,
            "p95_ticks": 0,
            "max_ticks": 0,
            "mean_ticks": 0,
            "max_events": 0,
        }
    ordered_ticks = sorted(burst_ticks)
    return {
        "count": len(burst_ticks),
        "p50_ticks": _quantile(ordered_ticks, 0.50),
        "p95_ticks": _quantile(ordered_ticks, 0.95),
        "max_ticks": ordered_ticks[-1],
        "mean_ticks": int(statistics.fmean(ordered_ticks)),
        "max_events": max(burst_events) if burst_events else 0,
    }


def _resume_suspend_balance(events):
    recs = {}
    for ev in events:
        thread = _event_thread_name(ev)
        name = _event_name(ev)
        row = recs.setdefault(thread, {"resume": 0, "suspend": 0})
        if name == "threadResume":
            row["resume"] += 1
        elif name == "threadSuspend":
            row["suspend"] += 1

    out = []
    for thread, row in recs.items():
        if thread in ("INTERRUPT", "<unknown>"):
            continue
        out.append(
            {
                "thread": thread,
                "resume": row["resume"],
                "suspend": row["suspend"],
                "delta": row["resume"] - row["suspend"],
            }
        )
    out.sort(key=lambda x: (abs(x["delta"]), x["resume"] + x["suspend"]), reverse=True)
    return out


def cmd_tracex_parse(
    input_file,
    top=20,
    show_pairs=20,
    quiet_warnings=True,
):
    try:
        from tracex_parser.file_parser import parse_tracex_buffer
    except ModuleNotFoundError as exc:
        raise RuntimeError(
            "Missing dependency 'tracex-parser'. Install with: pip install -r requirements.txt"
        ) from exc

    in_path = Path(input_file).resolve()
    if not in_path.exists():
        raise RuntimeError(f"TraceX file not found: {in_path}")

    if quiet_warnings:
        warning_buf = io.StringIO()
        with redirect_stdout(warning_buf), redirect_stderr(warning_buf):
            events, _obj_map = parse_tracex_buffer(str(in_path))
    else:
        warning_buf = None
        events, _obj_map = parse_tracex_buffer(str(in_path))
    if not events:
        print("[tracex-parse] no events parsed")
        return

    thread_counts = Counter()
    event_counts = Counter()
    for ev in events:
        thread_counts[_event_thread_name(ev)] += 1
        event_counts[_event_name(ev)] += 1
    pairs = _context_switch_pairs(events)
    runtime = _thread_runtime_slices(events)
    slice_stats = _slice_stats(runtime["slice_samples"])
    wait_balance = _wait_balance(events)
    irq_bursts = _interrupt_burst_stats(events)
    rs_balance = _resume_suspend_balance(events)

    print(f"[tracex-parse] parsed {len(events)} events from {in_path}")
    print()
    print(f"Top {top} threads by event count:")
    for name, count in thread_counts.most_common(top):
        print(f"  {count:8d}  {name}")

    print()
    print(f"Top {top} events by count:")
    for name, count in event_counts.most_common(top):
        print(f"  {count:8d}  {name}")

    if show_pairs > 0:
        print()
        print(f"Top {show_pairs} context-switch pairs (from -> to):")
        for (src, dst), count in pairs.most_common(show_pairs):
            print(f"  {count:8d}  {src} -> {dst}")

    total_ticks = runtime["total_ticks"] or 1
    print()
    print("Approx runtime by thread (timestamp delta attribution):")
    tick_rows = runtime["ticks_per_thread"].most_common(top)
    for thread, ticks in tick_rows:
        pct = 100.0 * ticks / total_ticks
        print(f"  {ticks:8d} ticks  ({pct:6.2f}%)  {thread}")
    if runtime["interrupt_ticks"] > 0:
        pct = 100.0 * runtime["interrupt_ticks"] / total_ticks
        print(f"  {runtime['interrupt_ticks']:8d} ticks  ({pct:6.2f}%)  INTERRUPT")
    if runtime["unknown_ticks"] > 0:
        pct = 100.0 * runtime["unknown_ticks"] / total_ticks
        print(f"  {runtime['unknown_ticks']:8d} ticks  ({pct:6.2f}%)  <unknown>")

    print()
    print(f"Top {min(top, len(slice_stats))} long-slice threads (p95 ticks):")
    for row in slice_stats[:top]:
        print(
            f"  p95={row['slice_p95_ticks']:6d}  p50={row['slice_p50_ticks']:6d}  "
            f"mean={row['slice_mean_ticks']:6d}  n={row['samples']:4d}  {row['thread']}"
        )

    print()
    print(f"Top {min(top, len(wait_balance))} wait-pressure threads:")
    for row in wait_balance[:top]:
        print(
            f"  sem={row['sem_wait_pressure']:4d} flags={row['flags_wait_pressure']:4d} "
            f"mtx={row['mtx_wait_pressure']:4d} sleep={row['sleep']:4d}  {row['thread']}"
        )

    print()
    print("Interrupt burst stats:")
    print(
        f"  bursts={irq_bursts['count']:4d}  p50={irq_bursts['p50_ticks']:8d}  "
        f"p95={irq_bursts['p95_ticks']:8d}  max={irq_bursts['max_ticks']:8d}  "
        f"mean={irq_bursts['mean_ticks']:8d}  max_events={irq_bursts['max_events']:4d}"
    )

    print()
    print(f"Top {min(top, len(rs_balance))} resume/suspend imbalance:")
    for row in rs_balance[:top]:
        print(
            f"  resume={row['resume']:4d} suspend={row['suspend']:4d} "
            f"delta={row['delta']:4d}  {row['thread']}"
        )

    if runtime["interrupt_ticks"] > total_ticks * 0.30:
        print()
        print("[tracex-parse][anomaly] high interrupt share (>30% attributed ticks)")
    if any(r["slice_p95_ticks"] > 50000 for r in slice_stats):
        print()
        print("[tracex-parse][anomaly] very long p95 run slices detected (>50k ticks)")
    if irq_bursts["p95_ticks"] > 500000:
        print()
        print("[tracex-parse][anomaly] long interrupt burst p95 detected (>500k ticks)")
    if any(abs(r["delta"]) > 10 for r in rs_balance):
        print()
        print("[tracex-parse][anomaly] threadResume/threadSuspend imbalance detected (|delta| > 10)")

    if warning_buf is not None:
        warnings = [
            line for line in warning_buf.getvalue().splitlines() if line.strip()
        ]
        if warnings:
            print()
            print(f"[tracex-parse] suppressed {len(warnings)} parser warning lines")

