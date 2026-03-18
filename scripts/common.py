import shutil
import stat
import subprocess
import sys


def run(args, **kwargs):
    """Run a subprocess, printing the command. Raises on failure."""
    print(f"  > {' '.join(str(a) for a in args)}")
    result = subprocess.run(args, **kwargs)
    if result.returncode != 0:
        cmd_str = " ".join(str(a) for a in args)
        raise RuntimeError(f"Command failed (exit {result.returncode}): {cmd_str}")


def require_tool(name):
    if shutil.which(name):
        return
    print(f"ERROR: '{name}' not found on PATH.", file=sys.stderr)
    sys.exit(1)


def remove_if_exists(path):
    """Delete a file, handling read-only flags (common on Windows build artifacts)."""
    if path.exists():
        path.chmod(path.stat().st_mode | stat.S_IWRITE)
        path.unlink()


def bin_to_hex(bin_file, hex_file, base_address):
    remove_if_exists(hex_file)
    run(
        [
            "arm-none-eabi-objcopy",
            "-I",
            "binary",
            "-O",
            "ihex",
            "--change-addresses",
            base_address,
            str(bin_file),
            str(hex_file),
        ]
    )
    if not hex_file.exists():
        raise RuntimeError(f"HEX conversion failed: {hex_file}")
