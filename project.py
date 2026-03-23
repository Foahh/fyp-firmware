#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path

from scripts.build import cmd_build
from scripts.clean import cmd_clean
from scripts.flash import cmd_flash
from scripts.format import cmd_format
from scripts.generate_model import cmd_model
from scripts.generate_proto import cmd_generate_proto

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

CAMERA_FPS_CHOICES = (10, 15, 20, 25, 30)
DEFAULT_CAMERA_FPS = 30


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

    model_parser = sub.add_parser(
        "model", help="Generate network model sources and binaries"
    )
    model_parser.add_argument(
        "--name",
        "-n",
        dest="model",
        choices=list(MODELS),
        default=DEFAULT_MODEL,
        help=f"Model to use (default: {DEFAULT_MODEL})",
    )

    proto_parser = sub.add_parser(
        "proto", help="Generate protobuf outputs (nanopb C + Python modules)"
    )
    proto_parser.add_argument(
        "--proto",
        nargs="?",
        const="",
        metavar="PROTO",
        help="Generate protobuf outputs (optionally for one .proto file)",
    )

    build_parser = sub.add_parser(
        "build", help="Build, sign, and convert to flashable HEX"
    )
    build_parser.add_argument(
        "--name",
        "-n",
        dest="model",
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
    build_parser.add_argument(
        "--fps",
        type=int,
        choices=CAMERA_FPS_CHOICES,
        default=DEFAULT_CAMERA_FPS,
        metavar="N",
        help=f"Camera frame rate (default: {DEFAULT_CAMERA_FPS})",
    )
    build_parser.add_argument(
        "--debug",
        action="store_true",
        help="Build in Debug mode (no sign/hex)",
    )
    build_parser.add_argument(
        "--appli",
        action="store_true",
        default=False,
        help="Build Appli (default: both Appli and FSBL unless one is specified)",
    )
    build_parser.add_argument(
        "--fsbl",
        action="store_true",
        default=False,
        help="Build FSBL (default: both Appli and FSBL unless one is specified)",
    )

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
    elif args.command == "proto":
        cmd_generate_proto(args.proto or None)
    elif args.command == "build":
        build_appli = args.appli or not args.fsbl
        build_fsbl = args.fsbl or not args.appli
        build_type = "Debug" if args.debug else BUILD_TYPE
        cmd_build(
            PROJECT_ROOT,
            PROJECTS,
            resolve_model(args),
            build_type,
            PROJECT_NAME_PREFIX,
            appli=build_appli,
            fsbl=build_fsbl,
            sign=not args.debug,
            force=args.force,
            snapshot=args.snapshot,
            performance=args.performance,
            camera_fps=args.fps,
        )
    elif args.command == "flash":
        cmd_flash(PROJECT_ROOT, BUILD_TYPE, PROJECT_NAME_PREFIX, force=args.force)


if __name__ == "__main__":
    main()
