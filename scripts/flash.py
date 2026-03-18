import shutil
import sys
from pathlib import Path

from .common import require_tool, run

EXTERNAL_LOADER_NAME = "MX66UW1G45G_STM32N6570-DK.stldr"


def find_external_loader():
    """Locate the STM32 external loader next to STM32_Programmer_CLI."""
    programmer = shutil.which("STM32_Programmer_CLI")
    if programmer is None:
        return None
    loader = Path(programmer).resolve().parent / "ExternalLoader" / EXTERNAL_LOADER_NAME
    return loader if loader.exists() else None


def flash_hex(label, hex_file):
    """Flash a single HEX image via SWD. Raises on failure."""
    if not hex_file.exists():
        raise RuntimeError(f"Image not found: {hex_file}")

    loader = find_external_loader()
    if loader is None:
        raise RuntimeError(
            f"External loader not found. "
            f"Expected: .../STM32CubeProgrammer/bin/ExternalLoader/{EXTERNAL_LOADER_NAME}"
        )

    print(f"\n=== Flashing {label} ===")
    run(
        [
            "STM32_Programmer_CLI",
            "-c",
            "port=SWD mode=HOTPLUG ap=1",
            "-el",
            str(loader),
            "-hardRst",
            "-w",
            str(hex_file),
        ]
    )


def cmd_flash(project_root, build_type, name_prefix):
    require_tool("STM32_Programmer_CLI")

    def hex_path(project):
        return (
            project_root
            / project
            / "build"
            / build_type
            / f"{name_prefix}{project}-trusted.hex"
        )

    flash_hex("FSBL", hex_path("FSBL"))

    # Network models — flash all .hex files in Networks/Bin
    bin_dir = project_root / "Networks" / "Bin"
    hex_files = sorted(bin_dir.glob("*.hex")) if bin_dir.exists() else []
    if not hex_files:
        print("WARNING: No network HEX files found, skipping.", file=sys.stderr)
    for hf in hex_files:
        flash_hex(f"Network: {hf.name}", hf)

    flash_hex("Appli", hex_path("Appli"))

    print("\n=== Flash completed successfully ===")
