"""Serial port detection and resolution."""

from typing import Optional

from serial.tools import list_ports


def _real_ports() -> list:
    return [p for p in list_ports.comports() if p.vid is not None]


def _score_port(
    info,
    keyword_scores: dict[str, int],
    preferred_vids: set[int] | None = None,
    rejected_vids: set[int] | None = None,
) -> int:
    """Score a serial port by VID and keyword matches in its metadata."""
    if rejected_vids and info.vid in rejected_vids:
        return -1
    score = 0
    if preferred_vids and info.vid in preferred_vids:
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


def _rank_ports(ports: list, scorer) -> list[tuple[object, int]]:
    """Return ports sorted by (score, device) descending, with cached scores."""
    scored = [(p, scorer(p)) for p in ports]
    scored.sort(key=lambda t: (t[1], t[0].device), reverse=True)
    return scored


def resolve_port(port: Optional[str]) -> str:
    if port:
        return port

    ports = _real_ports()
    if not ports:
        raise RuntimeError("No serial ports found. Pass port explicitly.")

    _ESPRESSIF_VIDS = {0x303A}
    _ST_VIDS = {0x0483}
    _FW_KEYWORDS = {
        "stm32": 6,
        "stmicro": 6,
        "stlink": 5,
        "nucleo": 4,
        "discovery": 4,
        "virtual com": 3,
        "vcp": 3,
        "usb serial": 1,
    }
    scorer = lambda info: _score_port(
        info, _FW_KEYWORDS, preferred_vids=_ST_VIDS, rejected_vids=_ESPRESSIF_VIDS
    )
    ranked = _rank_ports(ports, scorer)

    selected, selected_score = ranked[0]
    if selected_score <= 0 and len(ranked) > 1:
        candidates = ", ".join(p.device for p, _ in ranked[:4])
        print(
            f"[resolve_port] Could not confidently identify firmware port; choosing {selected.device} from: {candidates}"
        )
    else:
        print(
            f"[resolve_port] Auto-selected firmware-like port: {selected.device} (score={selected_score})"
        )
    return selected.device


def resolve_power_port(
    port: Optional[str],
    *,
    exclude: Optional[str] = None,
    baud: int = 921600,
    handshake_timeout_s: float = 2.0,
    probe_fn,
) -> Optional[str]:
    if port:
        return port

    ports = _real_ports()
    if not ports:
        return None

    for info in sorted(ports, key=lambda p: p.device):
        if exclude is not None and info.device == exclude:
            continue
        try:
            if probe_fn(info.device, baud, handshake_timeout_s):
                print(
                    f"[resolve_power_port] Auto-selected power monitor port: {info.device} (PM_PING)"
                )
                return info.device
        except Exception as exc:
            print(
                f"[resolve_power_port] PM_PING probe skipped {info.device}: {exc}",
            )
    return None
