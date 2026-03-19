import hashlib
import shutil
import sys
from pathlib import Path

from .common import require_tool, run

EXTERNAL_LOADER_NAME = "MX66UW1G45G_STM32N6570-DK.stldr"
FLASH_CACHE_FILE = ".flash_cache"


def find_external_loader():
    """Locate the STM32 external loader next to STM32_Programmer_CLI."""
    programmer = shutil.which("STM32_Programmer_CLI")
    if programmer is None:
        return None
    loader = Path(programmer).resolve().parent / "ExternalLoader" / EXTERNAL_LOADER_NAME
    return loader if loader.exists() else None


def file_hash(path):
    """Compute SHA-256 hex digest of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def read_cached_hash(hex_file):
    """Read the previously flashed hash for a HEX file, or None."""
    cache = hex_file.parent / FLASH_CACHE_FILE
    if not cache.exists():
        return None
    try:
        for line in cache.read_text().splitlines():
            name, _, digest = line.partition("=")
            if name.strip() == hex_file.name:
                return digest.strip()
    except OSError:
        pass
    return None


def write_cached_hash(hex_file, digest):
    """Write/update the flashed hash for a HEX file."""
    cache = hex_file.parent / FLASH_CACHE_FILE
    entries = {}
    if cache.exists():
        try:
            for line in cache.read_text().splitlines():
                name, _, d = line.partition("=")
                if name.strip():
                    entries[name.strip()] = d.strip()
        except OSError:
            pass
    entries[hex_file.name] = digest
    cache.write_text("\n".join(f"{k}={v}" for k, v in sorted(entries.items())) + "\n")


def flash_hex(label, hex_file, force=False):
    """Flash a single HEX image via SWD. Skips if unchanged. Raises on failure."""
    if not hex_file.exists():
        raise RuntimeError(f"Image not found: {hex_file}")

    digest = file_hash(hex_file)
    if not force and read_cached_hash(hex_file) == digest:
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
    write_cached_hash(hex_file, digest)


def cmd_flash(project_root, build_type, name_prefix, force=False):
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

    # Network models — flash all .hex files in Networks/Bin
    bin_dir = project_root / "Networks" / "Bin"
    hex_files = sorted(bin_dir.glob("*.hex")) if bin_dir.exists() else []
    if not hex_files:
        print("WARNING: No network HEX files found, skipping.", file=sys.stderr)
    for hf in hex_files:
        flash_hex(f"Network: {hf.name}", hf, force=force)

    flash_hex("Appli", hex_path("Appli"), force=force)

    print("\n=== Flash completed successfully ===")
