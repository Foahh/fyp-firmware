#!/usr/bin/env python3

import argparse
import sys
from pathlib import Path

from scripts.build import cmd_build
from scripts.clean import cmd_clean
from scripts.flash import cmd_flash
from scripts.format import cmd_format
from scripts.generate_model import cmd_model, cmd_model_all
from scripts.generate_proto import cmd_generate_proto
from scripts.tracex_dump import cmd_tracex_dump
from scripts.tracex_parse import cmd_tracex_parse
from scripts.ui import cmd_ui

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
    "yolox_480": {
        "define": "MODEL_YOLOX_NANO_480_D100_W025_PERSON",
        "model_file": "models/st_yoloxn_d100_w025_480_int8.tflite",
        "network_name": "st_yolo_x_480_person",
        "neural_art": "object_detection@my_neural_art.json",
        "sram_size_kb": 512,
    },
    "yolod_256": {
        "define": "MODEL_ST_YOLODV2MILLI_256_PERSON",
        "model_file": "models/st_yolodv2milli_actrelu_pt_coco_person_256_qdq_int8.onnx",
        "network_name": "st_yolo_d_256_person",
        "neural_art": "object_detection@my_neural_art.json",
        "sram_size_kb": 1024,
        "inputs_ch_position": "chlast",
    },
    "yolo26_320": {
        "define": "MODEL_YOLO26_320_PERSON",
        "model_file": "models/yolo26_320_qdq_int8_od_coco-person-st.onnx",
        "network_name": "yolo26_320_person",
        "neural_art": "object_detection@my_neural_art.json",
        "sram_size_kb": 512,
        "inputs_ch_position": "chlast",
    },
}

DEFAULT_MODEL = "yolox_480"

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
    model_name_group = model_parser.add_mutually_exclusive_group(required=False)
    model_name_group.add_argument(
        "--name",
        "-n",
        dest="model",
        choices=list(MODELS),
        help=f"Model to generate (default: {DEFAULT_MODEL} if --all not set)",
    )
    model_name_group.add_argument(
        "--all",
        action="store_true",
        dest="model_all",
        help="Generate every model; deletes Networks/Src and Networks/Bin first",
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

    build_parser = sub.add_parser("build", help="Build firmware")
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
        "--mode",
        choices=["underdrive", "nominal", "overdrive"],
        default="underdrive",
        help="Power/clock mode (default: underdrive)",
    )
    build_parser.add_argument(
        "--fps",
        type=int,
        default=DEFAULT_CAMERA_FPS,
        metavar="N",
        help=f"Camera frame rate (choices: {CAMERA_FPS_CHOICES}, default: {DEFAULT_CAMERA_FPS})",
    )
    build_parser.add_argument(
        "--debug",
        action="store_true",
        help="Build in Debug mode",
    )
    build_parser.add_argument(
        "--snapshot",
        action="store_true",
        help="NN uses single-frame camera snapshots instead of continuous capture",
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
    build_parser.add_argument(
        "--flash",
        action="store_true",
        help="Flash firmware to device after building",
    )
    build_parser.add_argument(
        "--tracex",
        action="store_true",
        help="Enable ThreadX TraceX event tracing support in Appli",
    )

    ui_parser = sub.add_parser("ui", help="Launch host serial visualizer")
    ui_parser.add_argument(
        "port",
        nargs="?",
        default=None,
        help="Serial port (optional; auto-detected if omitted)",
    )
    ui_parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=921600,
        help="Baud rate (default: 921600)",
    )
    ui_parser.add_argument(
        "--timeout",
        type=float,
        default=2.0,
        help="Serial read timeout in seconds (default: 2.0)",
    )

    tracex_parser = sub.add_parser(
        "tracex", help="Request and save TraceX dump over serial"
    )
    tracex_parser.add_argument(
        "port",
        nargs="?",
        default=None,
        help="Serial port (optional; auto-detected if omitted)",
    )
    tracex_parser.add_argument(
        "-o",
        "--output",
        default="tracex_dump.bin",
        help="Output file for raw TraceX buffer (default: tracex_dump.bin)",
    )
    tracex_parser.add_argument(
        "--chunk-size",
        type=int,
        default=256,
        help="Requested chunk size in bytes (default: 256)",
    )
    tracex_parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=921600,
        help="Baud rate (default: 921600)",
    )
    tracex_parser.add_argument(
        "--timeout",
        type=float,
        default=2.0,
        help="Serial read timeout in seconds (default: 2.0)",
    )

    tracex_parse_parser = sub.add_parser(
        "tracex-parse", help="Parse a TraceX dump and print thread/event hotspots"
    )
    tracex_parse_parser.add_argument(
        "input",
        nargs="?",
        default="tracex_dump.bin",
        help="Input TraceX dump file (default: tracex_dump.bin)",
    )
    tracex_parse_parser.add_argument(
        "--top",
        type=int,
        default=20,
        help="Number of top rows to print (default: 20)",
    )
    tracex_parse_parser.add_argument(
        "--pairs",
        type=int,
        default=20,
        help="Top context-switch pairs to print (default: 20)",
    )
    tracex_parse_parser.add_argument(
        "--show-parser-warnings",
        action="store_true",
        help="Show raw tracex-parser warnings instead of suppressing them",
    )

    args = parser.parse_args()

    if args.command == "clean":
        cmd_clean(PROJECT_ROOT)
    elif args.command == "format":
        cmd_format(PROJECT_ROOT)
    elif args.command == "model":
        if args.model_all:
            cmd_model_all(PROJECT_ROOT, MODELS, NETWORK_BIN_ADDRESS)
        else:
            model_key = args.model if args.model is not None else DEFAULT_MODEL
            if model_key not in MODELS:
                print(
                    f"ERROR: Unknown model '{model_key}'. Available: {', '.join(MODELS)}",
                    file=sys.stderr,
                )
                sys.exit(1)
            cmd_model(PROJECT_ROOT, MODELS[model_key], model_key, NETWORK_BIN_ADDRESS)
    elif args.command == "proto":
        cmd_generate_proto(args.proto or None)
    elif args.command == "build":
        if args.fps not in CAMERA_FPS_CHOICES:
            parser.error(f"--fps must be one of {CAMERA_FPS_CHOICES}")

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
            sign=args.flash,
            force=args.force,
            snapshot=args.snapshot,
            power_mode=args.mode,
            camera_fps=args.fps,
            tracex=args.tracex,
        )
        if args.flash:
            m = resolve_model(args)
            cmd_flash(
                PROJECT_ROOT,
                build_type,
                PROJECT_NAME_PREFIX,
                m["network_name"],
                force=args.force,
            )
    elif args.command == "ui":
        cmd_ui(PROJECT_ROOT, args.port, baud=args.baud, timeout=args.timeout)
    elif args.command == "tracex":
        cmd_tracex_dump(
            PROJECT_ROOT,
            args.port,
            args.output,
            chunk_size=args.chunk_size,
            baud=args.baud,
            timeout=args.timeout,
        )
    elif args.command == "tracex-parse":
        try:
            cmd_tracex_parse(
                args.input,
                top=args.top,
                show_pairs=args.pairs,
                quiet_warnings=not args.show_parser_warnings,
            )
        except RuntimeError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
