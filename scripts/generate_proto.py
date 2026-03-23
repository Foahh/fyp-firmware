#!/usr/bin/env python3
"""Regenerate nanopb C files and Python protobuf modules from .proto sources."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROTO_DIR = ROOT / "Appli" / "Proto"
OUT_DIR = PROTO_DIR
NANOPB_PROTO = ROOT / "External" / "nanopb" / "generator" / "proto"


def resolve_nanopb_plugin():
    generator_dir = ROOT / "External" / "nanopb" / "generator"
    if sys.platform.startswith("win"):
        plugin = generator_dir / "protoc-gen-nanopb.bat"
    else:
        plugin = generator_dir / "protoc-gen-nanopb"

    if not plugin.exists():
        print(f"ERROR: nanopb plugin not found at {plugin}", file=sys.stderr)
        sys.exit(1)

    return plugin


NANOPB_PLUGIN = resolve_nanopb_plugin()

PROTO_FILES = list(PROTO_DIR.glob("*.proto"))


def run(cmd):
    print(f"  {' '.join(str(c) for c in cmd)}")
    subprocess.check_call(cmd)


def resolve_proto_files(proto_arg):
    if not PROTO_FILES:
        return []

    if not proto_arg:
        return PROTO_FILES

    requested = Path(proto_arg)
    if requested.suffix != ".proto":
        requested = requested.with_suffix(".proto")

    if not requested.is_absolute():
        # Support both --proto messages.proto and --proto Appli/Proto/messages.proto.
        candidate = (ROOT / requested).resolve()
        if candidate.parent != PROTO_DIR.resolve():
            candidate = (PROTO_DIR / requested.name).resolve()
    else:
        candidate = requested.resolve()

    available = {p.resolve() for p in PROTO_FILES}
    if candidate not in available:
        print(
            f"ERROR: .proto file not found: {proto_arg}. Available: "
            + ", ".join(sorted(p.name for p in PROTO_FILES)),
            file=sys.stderr,
        )
        sys.exit(1)

    return [candidate]


def cmd_generate_proto(proto=None):
    proto_files = resolve_proto_files(proto)
    if not proto_files:
        print("No .proto files found in", PROTO_DIR)
        sys.exit(1)

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for proto in proto_files:
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
    cmd_generate_proto()
