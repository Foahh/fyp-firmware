import shutil
import sys

from .common import bin_to_hex, run


def cmd_model(project_root, model, network_bin_address):
    network_name = model["network_name"]
    networks_dir = project_root / "Networks"

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
            "stedgeai",
            "generate",
            "--no-inputs-allocation",
            "--no-outputs-allocation",
            "--model",
            str(networks_dir / model["tflite"]),
            "--target",
            "stm32n6",
            "--st-neural-art",
            str(networks_dir / model["neural_art"]),
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
