#!/usr/bin/env python3
"""Regenerate nanopb C files and Python protobuf modules from .proto sources."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROTO_DIR = ROOT / "Appli" / "Proto"
OUT_DIR = PROTO_DIR / "nanopb"
NANOPB_PROTO = ROOT / "External" / "nanopb" / "generator" / "proto"
NANOPB_PLUGIN = ROOT / "External" / "nanopb" / "generator" / "protoc-gen-nanopb.bat"

PROTO_FILES = list(PROTO_DIR.glob("*.proto"))


def run(cmd):
    print(f"  {' '.join(str(c) for c in cmd)}")
    subprocess.check_call(cmd)


def main():
    if not PROTO_FILES:
        print("No .proto files found in", PROTO_DIR)
        sys.exit(1)

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for proto in PROTO_FILES:
        name = proto.stem
        print(f"\n=== {name}.proto ===")

        # nanopb C files
        run(
            [
                "protoc",
                f"--plugin=protoc-gen-nanopb={NANOPB_PLUGIN}",
                f"--nanopb_out={OUT_DIR}",
                f"-I{PROTO_DIR}",
                f"-I{NANOPB_PROTO}",
                str(proto),
            ]
        )

        # Python protobuf module
        run(
            [
                "protoc",
                f"--python_out={OUT_DIR}",
                f"-I{PROTO_DIR}",
                f"-I{NANOPB_PROTO}",
                str(proto),
            ]
        )

    # nanopb_pb2.py (dependency for generated Python modules)
    print("\n=== nanopb_pb2.py ===")
    run(
        [
            "protoc",
            f"--python_out={OUT_DIR}",
            f"-I{NANOPB_PROTO}",
            str(NANOPB_PROTO / "nanopb.proto"),
        ]
    )

    print("\nDone.")


if __name__ == "__main__":
    main()
