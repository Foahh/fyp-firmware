import sys

from .common import run


def cmd_ui(project_root, port=None, baud=115200, timeout=2.0):
    visualizer = project_root / "scripts" / "visualizer.py"
    cmd = [sys.executable, str(visualizer)]
    if port:
        cmd.append(str(port))
    cmd.extend(["--baud", str(baud), "--timeout", str(timeout)])
    run(cmd)
