import shutil
import sys
import os
import platform
from pathlib import Path

from .common import bin_to_hex, run


def resolve_stedgeai_path():
    core_dir = os.environ.get("STEDGEAI_CORE_DIR")
    if not core_dir:
        print(
            "ERROR: STEDGEAI_CORE_DIR is not set. "
            "Expected a path like /home/fn/ST/STEdgeAI/4.0.",
            file=sys.stderr,
        )
        sys.exit(1)

    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "linux":
        platform_dir = "linux"
    elif system == "darwin":
        platform_dir = "macarm" if machine in ("arm64", "aarch64") else "mac"
    elif system == "windows":
        platform_dir = "windows"
    else:
        print(f"ERROR: Unsupported host platform: {platform.system()}", file=sys.stderr)
        sys.exit(1)

    exe_name = "stedgeai.exe" if platform_dir == "windows" else "stedgeai"
    stedgeai_path = Path(core_dir) / "Utilities" / platform_dir / exe_name
    if not stedgeai_path.exists():
        print(
            f"ERROR: stedgeai not found at: {stedgeai_path}\n"
            "Check STEDGEAI_CORE_DIR and host platform directory under Utilities.",
            file=sys.stderr,
        )
        sys.exit(1)

    return stedgeai_path


def resolve_neural_art_arg(networks_dir, neural_art):
    if "@" in neural_art:
        profile, config_file = neural_art.split("@", 1)
        config_path = Path(config_file)
        if not config_path.is_absolute():
            config_path = networks_dir / config_file
        return f"{profile}@{config_path}"

    config_path = Path(neural_art)
    if not config_path.is_absolute():
        config_path = networks_dir / neural_art
    return str(config_path)


def cmd_model(project_root, model, network_bin_address):
    network_name = model["network_name"]
    networks_dir = project_root / "Networks"
    stedgeai = resolve_stedgeai_path()

    src_dir = networks_dir / "Src"
    bin_dir = networks_dir / "Bin"
    ai_output = networks_dir / "st_ai_output"

    # Reset output directories
    for d in (src_dir, bin_dir, ai_output, networks_dir / "st_ai_ws"):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True, exist_ok=True)

    print(f"\n=== Generating network: {network_name} ===")
    run(
        [
            str(stedgeai),
            "generate",
            "--no-inputs-allocation",
            "--no-outputs-allocation",
            "--model",
            str(networks_dir / model["tflite"]),
            "--target",
            "stm32n6",
            "--st-neural-art",
            resolve_neural_art_arg(networks_dir, model["neural_art"]),
            "--name",
            network_name,
            "--input-data-type",
            "uint8",
            "--output-data-type",
            "int8",
        ],
        cwd=str(networks_dir),
    )

    # Copy generated sources
    expected_files = [
        f"{network_name}_ecblobs.h",
        f"{network_name}.c",
        f"{network_name}.h",
        f"stai_{network_name}.c",
        f"stai_{network_name}.h",
    ]
    for name in expected_files:
        src = ai_output / name
        if src.exists():
            shutil.copy2(src, src_dir / name)
        else:
            print(f"WARNING: Expected output not found: {name}", file=sys.stderr)

    # Convert network binary to HEX
    bin_name = f"{network_name}_atonbuf.xSPI2"
    raw_file = ai_output / f"{bin_name}.raw"
    if not raw_file.exists():
        print(f"ERROR: Network binary not found: {raw_file}", file=sys.stderr)
        sys.exit(1)

    bin_file = bin_dir / f"{bin_name}.bin"
    hex_file = bin_dir / f"{bin_name}.hex"
    shutil.copy2(raw_file, bin_file)
    bin_to_hex(bin_file, hex_file, network_bin_address)
    print(f"Network HEX created: {hex_file}")
