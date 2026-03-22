#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path

from scripts.build import cmd_build
from scripts.clean import cmd_clean
from scripts.flash import cmd_flash
from scripts.format import cmd_format
from scripts.model import cmd_model

PROJECT_ROOT = Path(__file__).resolve().parent
PROJECT_NAME_PREFIX = "Firmware_"
BUILD_TYPE = "Release"

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

NETWORK_BIN_ADDRESS = "0x70380000"

MODELS = {
    "yolox_nano": {
        "define": "MODEL_YOLOX_NANO",
        "tflite": "models/st_yolo_x_nano_480_1.0_0.25_3_int8.tflite",
        "network_name": "od_yolo_x_person",
        "neural_art": "object_detection@my_neural_art.json",
    },
}

DEFAULT_MODEL = "yolox_nano"


def resolve_model(args):
    key = getattr(args, "model", DEFAULT_MODEL) or DEFAULT_MODEL
    if key not in MODELS:
        print(
            f"ERROR: Unknown model '{key}'. Available: {', '.join(MODELS)}",
            file=sys.stderr,
        )
        sys.exit(1)
    return MODELS[key]


def main():
    parser = argparse.ArgumentParser(description="STM32 firmware build tooling")
    sub = parser.add_subparsers(dest="command")
    sub.required = True

    sub.add_parser("clean", help="Remove build artifacts")
    sub.add_parser("format", help="Format all source files with clang-format")

    gen_parser = sub.add_parser(
        "gen", help="Generate network model sources and binaries"
    )
    gen_parser.add_argument(
        "--model",
        "-m",
        choices=list(MODELS),
        default=DEFAULT_MODEL,
        help=f"Model to use (default: {DEFAULT_MODEL})",
    )

    build_parser = sub.add_parser(
        "build", help="Build, sign, and convert to flashable HEX"
    )
    build_parser.add_argument(
        "--model",
        "-m",
        choices=list(MODELS),
        default=DEFAULT_MODEL,
        help=f"Model to use (default: {DEFAULT_MODEL})",
    )
    build_parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Re-sign firmware even if unchanged (ignore sign cache)",
    )
    build_parser.add_argument(
        "--snapshot",
        action="store_true",
        help="Use snapshot mode for NN camera pipe",
    )
    build_parser.add_argument(
        "--performance",
        action="store_true",
        help="Run NPU at 1000 MHz instead of 800 MHz",
    )

    build_appli_parser = sub.add_parser(
        "build-appli-debug", help="Build Appli only (Debug, no sign/hex)"
    )
    build_appli_parser.add_argument(
        "--model",
        "-m",
        choices=list(MODELS),
        default=DEFAULT_MODEL,
        help=f"Model to use (default: {DEFAULT_MODEL})",
    )
    build_appli_parser.add_argument(
        "--snapshot",
        action="store_true",
        help="Use snapshot mode for NN camera pipe",
    )
    build_appli_parser.add_argument(
        "--performance",
        action="store_true",
        help="Run NPU at 1000 MHz instead of 800 MHz",
    )

    sub.add_parser("build-fsbl-debug", help="Build FSBL only (Debug, no sign/hex)")

    flash_parser = sub.add_parser("flash", help="Flash pre-built firmware to device")
    flash_parser.add_argument(
        "--force", "-f", action="store_true", help="Flash all images even if unchanged"
    )

    args = parser.parse_args()

    if args.command == "clean":
        cmd_clean(PROJECT_ROOT)
    elif args.command == "format":
        cmd_format(PROJECT_ROOT)
    elif args.command == "model":
        cmd_model(PROJECT_ROOT, resolve_model(args), NETWORK_BIN_ADDRESS)
    elif args.command == "build":
        cmd_build(
            PROJECT_ROOT,
            PROJECTS,
            resolve_model(args),
            BUILD_TYPE,
            PROJECT_NAME_PREFIX,
            force=args.force,
            snapshot=args.snapshot,
            performance=args.performance,
        )
    elif args.command == "build-appli-debug":
        cmd_build(
            PROJECT_ROOT,
            PROJECTS,
            resolve_model(args),
            "Debug",
            PROJECT_NAME_PREFIX,
            fsbl=False,
            sign=False,
            snapshot=args.snapshot,
            performance=args.performance,
        )
    elif args.command == "build-fsbl-debug":
        cmd_build(
            PROJECT_ROOT,
            PROJECTS,
            resolve_model(args),
            "Debug",
            PROJECT_NAME_PREFIX,
            appli=False,
            sign=False,
        )
    elif args.command == "flash":
        cmd_flash(PROJECT_ROOT, BUILD_TYPE, PROJECT_NAME_PREFIX, force=args.force)


if __name__ == "__main__":
    main()
