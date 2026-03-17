#!/usr/bin/env python3

import argparse
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

# ── Constants

PROJECT_ROOT = Path(__file__).resolve().parent
PROJECT_NAME_PREFIX = "Firmware_"
BUILD_TYPE = "Release"

SIGNING_TOOL = "STM32_SigningTool_CLI"
FLASH_TOOL = "STM32_Programmer_CLI"
OBJCOPY_TOOL = "arm-none-eabi-objcopy"
EXTERNAL_LOADER_NAME = "MX66UW1G45G_STM32N6570-DK.stldr"

PROJECTS = {
    "FSBL": {
        "sub_dir": "FSBL",
        "flash_address": "0x70000000",
        "signing_type": "fsbl",
        "offset_address": "0x80000000",
    },
    "Appli": {
        "sub_dir": "Appli",
        "flash_address": "0x70100000",
        "signing_type": "fsbl",
        "offset_address": "0x80000000",
    },
}

# Network generate configuration
NETWORK_MODEL = "models/st_yolo_x_nano_480_1.0_0.25_3_int8.tflite"
NETWORK_NAME = "od_yolo_x_person"
NETWORK_NEURAL_ART = "object_detection@my_neural_art.json"
NETWORK_SRC_FILES = [
    "od_yolo_x_person_ecblobs.h",
    "od_yolo_x_person.c",
    "od_yolo_x_person.h",
    "stai_od_yolo_x_person.c",
    "stai_od_yolo_x_person.h",
]
NETWORK_BIN_NAME = "od_yolo_x_person_atonbuf.xSPI2"
NETWORK_BIN_ADDRESS = "0x70380000"

# ── Utilities


def run(args, cwd=None):
    """Run a subprocess, printing output in real time. Raises on failure."""
    print(f"  > {' '.join(str(a) for a in args)}")
    result = subprocess.run(args, cwd=cwd)
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed (exit {result.returncode}): {' '.join(str(a) for a in args)}"
        )


def require_tool(name):
    if shutil.which(name) is None:
        print(f"ERROR: Required tool not found: {name}. Please ensure it's in your PATH.", file=sys.stderr)
        sys.exit(1)


def find_external_loader():
    """Locate the STM32 external loader via STM32CLT_PATH env variable."""
    clt_path = os.environ.get("STM32CLT_PATH")
    if not clt_path:
        return None
    loader = Path(clt_path) / "STM32CubeProgrammer" / "bin" / "ExternalLoader" / EXTERNAL_LOADER_NAME
    return loader if loader.exists() else None


# ── Clean command


def cmd_clean(_args):
    dirs = [
        PROJECT_ROOT / "FSBL" / "build",
        PROJECT_ROOT / "Appli" / "build",
        PROJECT_ROOT / "Appli" / ".cache",
        PROJECT_ROOT / "build",
    ]
    for d in dirs:
        if d.exists():
            shutil.rmtree(d)
            print(f"Removed: {d}")
        else:
            print(f"Not found: {d}")
    print("Artifacts cleaned.")


# ── Generate command


def cmd_generate_network(_args):
    networks_dir = PROJECT_ROOT / "Networks"
    src_dir = networks_dir / "Src"
    bin_dir = networks_dir / "Bin"
    ai_output = networks_dir / "st_ai_output"
    ai_ws = networks_dir / "st_ai_ws"

    for d in [src_dir, bin_dir, ai_output, ai_ws]:
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True, exist_ok=True)

    require_tool("stedgeai")

    print("\n=== Generating network model ===")
    run(
        [
            "stedgeai", "generate",
            "--no-inputs-allocation",
            "--no-outputs-allocation",
            "--model", str(networks_dir / NETWORK_MODEL),
            "--target", "stm32n6",
            "--st-neural-art", str(networks_dir / NETWORK_NEURAL_ART),
            "--name", NETWORK_NAME,
            "--input-data-type", "uint8",
            "--output-data-type", "int8",
        ],
        cwd=str(networks_dir),
    )

    print("\n=== Copying generated sources ===")
    for name in NETWORK_SRC_FILES:
        src = ai_output / name
        if src.exists():
            shutil.copy2(src, src_dir / name)
            print(f"Copied: {name}")
        else:
            print(f"WARNING: Expected output not found: {name}", file=sys.stderr)

    print("\n=== Converting network binary to HEX ===")
    raw_file = ai_output / f"{NETWORK_BIN_NAME}.raw"
    bin_file = bin_dir / f"{NETWORK_BIN_NAME}.bin"
    hex_file = bin_dir / f"{NETWORK_BIN_NAME}.hex"

    if raw_file.exists():
        shutil.copy2(raw_file, bin_file)
    else:
        print(f"ERROR: Network binary not found: {raw_file}", file=sys.stderr)
        sys.exit(1)

    require_tool(OBJCOPY_TOOL)
    run([
        OBJCOPY_TOOL, "-I", "binary",
        str(bin_file),
        "--change-addresses", NETWORK_BIN_ADDRESS,
        "-O", "ihex",
        str(hex_file),
    ])
    print(f"HEX created: {hex_file}")


# ── Build command


def _build_project(name, cfg):
    """Configure, compile, sign, and convert a single project to flashable HEX."""
    project_dir = PROJECT_ROOT / cfg["sub_dir"]
    build_dir = PROJECT_ROOT / cfg["sub_dir"] / "build" / BUILD_TYPE
    full_name = f"{PROJECT_NAME_PREFIX}{name}"

    print(f"\n=== Configuring {name} ({BUILD_TYPE}) ===")
    run(["cmake", "--preset", BUILD_TYPE], cwd=str(project_dir))

    print(f"\n=== Building {name} ({BUILD_TYPE}) ===")
    run(["cmake", "--build", "--preset", BUILD_TYPE], cwd=str(project_dir))
    print(f"{name} built successfully")

    # Clean old artifacts
    print(f"\n=== Cleaning old {name} build artifacts ===")
    for suffix in ["-trusted.bin", ".bin"]:
        old = build_dir / f"{full_name}{suffix}"
        if old.exists():
            old.chmod(old.stat().st_mode | stat.S_IWRITE)
            old.unlink()
            print(f"Removed: {old}")

    # ELF → binary
    print(f"\n=== Converting {name} ELF to binary ===")
    elf = build_dir / f"{full_name}.elf"
    bin_file = build_dir / f"{full_name}.bin"
    if not elf.exists():
        raise RuntimeError(f"ELF file not found: {elf}")
    run([OBJCOPY_TOOL, "-O", "binary", str(elf), str(bin_file)])
    print(f"Converted ELF to binary: {bin_file}")

    # Sign
    signed_bin = build_dir / f"{full_name}-trusted.bin"
    print(f"\n=== Signing {name} ===")
    print(f"Using binary file: {bin_file}")
    if signed_bin.exists():
        signed_bin.unlink()
    print(f"Signing binary with type: {cfg['signing_type']}...")
    run([
        SIGNING_TOOL,
        "-bin", str(bin_file),
        "-nk",
        "-of", cfg["offset_address"],
        "-t", cfg["signing_type"],
        "-o", str(signed_bin),
        "-hv", "2.3",
        "-dump", str(signed_bin),
        "-align",
    ])
    print(f"Signed binary created: {signed_bin}")

    # Convert to HEX
    signed_hex = build_dir / f"{full_name}-trusted.hex"
    print(f"\n=== Converting signed binary to HEX (base address {cfg['flash_address']}) ===")
    if signed_hex.exists():
        signed_hex.unlink()
    run([
        OBJCOPY_TOOL, "-I", "binary", "-O", "ihex",
        "--change-addresses", cfg["flash_address"],
        str(signed_bin), str(signed_hex),
    ])
    if not signed_hex.exists():
        raise RuntimeError(f"Failed to convert signed binary to HEX for {name}")
    print(f"HEX image created: {signed_hex}")


def cmd_build(_args):
    require_tool(SIGNING_TOOL)
    require_tool(OBJCOPY_TOOL)

    print("STM32 Build and Sign")
    print(f"Project Root: {PROJECT_ROOT}")
    print(f"Build Type: {BUILD_TYPE}")

    all_ok = True

    print("\n=== Processing FSBL ===")
    try:
        _build_project("FSBL", PROJECTS["FSBL"])
    except RuntimeError as e:
        print(f"ERROR: FSBL failed: {e}", file=sys.stderr)
        all_ok = False

    print("\n=== Processing Appli ===")
    try:
        _build_project("Appli", PROJECTS["Appli"])
    except RuntimeError as e:
        print(f"ERROR: Appli failed: {e}", file=sys.stderr)
        all_ok = False

    if all_ok:
        print("\n=== Build completed successfully ===")
    else:
        print("\n=== Build completed with errors ===", file=sys.stderr)
        sys.exit(1)


# ── Flash command


def _flash_hex(label, hex_file):
    """Flash a single HEX image. Returns True on success."""
    print(f"\n=== Flashing {label} ===")
    if not hex_file.exists():
        print(f"ERROR: Image file not found: {hex_file}", file=sys.stderr)
        return False

    print(f"Using image file: {hex_file}")

    loader = find_external_loader()
    if loader is None:
        print(f"WARNING: External loader not found. Attempting without it (may fail for external flash).", file=sys.stderr)
        print(f"WARNING: Expected: .../STM32CubeProgrammer/bin/ExternalLoader/{EXTERNAL_LOADER_NAME}", file=sys.stderr)
    else:
        print(f"Using external loader: {loader}")

    print("Flashing HEX image to external flash...")
    flash_args = [FLASH_TOOL, "-c", "port=SWD mode=HOTPLUG ap=1"]
    if loader:
        flash_args += ["-el", str(loader)]
    flash_args += ["-hardRst", "-w", str(hex_file)]

    try:
        run(flash_args)
    except RuntimeError:
        print(f"ERROR: Failed to flash {label}", file=sys.stderr)
        return False

    print(f"{label} flashed successfully")
    return True


def _flash_network_models():
    """Flash all .hex files in Networks/Bin. Returns True if all succeed."""
    print("\n=== Flashing Network Models ===")
    bin_dir = PROJECT_ROOT / "Networks" / "Bin"
    if not bin_dir.exists():
        print(f"WARNING: Network Bin directory not found: {bin_dir}", file=sys.stderr)
        print("WARNING: Skipping network model flash. Please generate the network models first.", file=sys.stderr)
        return False

    hex_files = sorted(bin_dir.glob("*.hex"))
    if not hex_files:
        print(f"WARNING: No network HEX files found in: {bin_dir}", file=sys.stderr)
        print("WARNING: Skipping network model flash. Please generate the network models first.", file=sys.stderr)
        return False

    print(f"Found {len(hex_files)} network HEX file(s) to flash")
    all_ok = True
    for hf in hex_files:
        if not _flash_hex(f"Network Model: {hf.name}", hf):
            all_ok = False

    return all_ok


def cmd_flash(_args):
    require_tool(FLASH_TOOL)

    print("STM32 Flash")
    print(f"Project Root: {PROJECT_ROOT}")

    all_ok = True

    # 1. FSBL
    fsbl_hex = PROJECT_ROOT / "FSBL" / "build" / BUILD_TYPE / f"{PROJECT_NAME_PREFIX}FSBL-trusted.hex"
    if not _flash_hex("FSBL", fsbl_hex):
        all_ok = False

    # 2. Network models (failure is a warning, not fatal)
    if not _flash_network_models():
        print("WARNING: Network model flash failed or was skipped, but continuing...", file=sys.stderr)

    # 3. Appli
    appli_hex = PROJECT_ROOT / "Appli" / "build" / BUILD_TYPE / f"{PROJECT_NAME_PREFIX}Appli-trusted.hex"
    if not _flash_hex("Appli", appli_hex):
        all_ok = False

    if all_ok:
        print("\n=== Flash completed successfully ===")
    else:
        print("\n=== Flash completed with errors ===", file=sys.stderr)
        sys.exit(1)


# ── Main


def main():
    parser = argparse.ArgumentParser(
        description="STM32 firmware build tooling"
    )
    sub = parser.add_subparsers(dest="command")
    sub.required = True

    sub.add_parser("clean", help="Remove build artifacts")
    sub.add_parser("gen", help="Generate network model sources and binaries")
    sub.add_parser("build", help="Build, sign, and convert firmware to flashable HEX")
    sub.add_parser("flash", help="Flash pre-built firmware to device")

    args = parser.parse_args()
    {"clean": cmd_clean, "gen": cmd_generate_network, "build": cmd_build, "flash": cmd_flash}[args.command](args)


if __name__ == "__main__":
    main()
