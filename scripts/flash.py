import shutil
from pathlib import Path

from .common import file_hash, read_cached_hash, require_tool, run, write_cached_hash

EXTERNAL_LOADER_NAME = "MX66UW1G45G_STM32N6570-DK.stldr"
FLASH_CACHE_FILE = ".flash_cache"
LAST_NETWORK_FLASHED_FILE = ".last_flashed_network"


def find_external_loader():
    """Locate the STM32 external loader next to STM32_Programmer_CLI."""
    programmer = shutil.which("STM32_Programmer_CLI")
    if programmer is None:
        return None
    loader = Path(programmer).resolve().parent / "ExternalLoader" / EXTERNAL_LOADER_NAME
    return loader if loader.exists() else None


def flash_hex(label, hex_file, force=False):
    """Flash a single HEX image via SWD. Skips if unchanged. Raises on failure."""
    if not hex_file.exists():
        raise RuntimeError(f"Image not found: {hex_file}")

    cache = hex_file.parent / FLASH_CACHE_FILE
    digest = file_hash(hex_file)
    if not force and read_cached_hash(hex_file, cache) == digest:
        print(f"\n--- Skipping {label} (unchanged) ---")
        return

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
    write_cached_hash(hex_file, digest, cache)


def cmd_flash(project_root, build_type, name_prefix, network_name, force=False):
    require_tool("STM32_Programmer_CLI")

    def hex_path(project):
        return (
            project_root
            / project
            / "build"
            / build_type
            / f"{name_prefix}{project}-trusted.hex"
        )

    flash_hex("FSBL", hex_path("FSBL"), force=force)

    bin_dir = project_root / "Networks" / "Bin"
    net_hex = bin_dir / f"{network_name}_atonbuf.xSPI2.hex"
    if not net_hex.exists():
        raise RuntimeError(
            f"Network image not found: {net_hex}\n"
            f"Run: python project.py model -n <model>  (see project.py MODELS for keys)"
        )
    last_net_path = bin_dir / LAST_NETWORK_FLASHED_FILE
    prev_net = (
        last_net_path.read_text(encoding="utf-8").strip()
        if last_net_path.exists()
        else ""
    )
    network_force = force or (prev_net != net_hex.name)
    flash_hex(f"Network: {net_hex.name}", net_hex, force=network_force)
    last_net_path.write_text(net_hex.name + "\n", encoding="utf-8")

    flash_hex("Appli", hex_path("Appli"), force=force)

    print("\n=== Flash completed successfully ===")
