# FYP - Embedded person-detection & proximity-alert firmware

Firmware for the **STM32N6570-DK** (Arm Cortex-M55 application core plus integrated **Neural-ART** accelerator). It ingests RGB frames from the **B-CAM-IMX** Sony **IMX335** over MIPI CSI, runs compile-time-selectable **person detection** on the NPU, draws overlays on the kit LCD, and streams **length-prefixed nanopb** telemetry to a host over UART—all under **Azure RTOS ThreadX** in a pipelined, multi-threaded layout. A **person-distance proximity alert** fuses detections with the onboard **VL53L5CX** multizone ToF **8×8** depth grid; a **ByteTrack-derived** tracker maintains temporal association. Optional **board-level power** uses an **INA228** with an **ESP32-C6** front end (second UART, GPIO-gated inference windows), consumed by the same **`project.py ui`** visualizer toolchain.

## Build

### Prerequisites

```sh
git submodule update --init --recursive
pip install -r requirements.txt
```

Install [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) and [STEdgeAI-Core](https://www.st.com/en/development-tools/stedgeai-core.html).

Ensure the following tools are available:

- `cmake`, `ninja`
- `arm-none-eabi-*`
- `STM32_SigningTool_CLI`, `STM32_Programmer_CLI` (only when producing signed HEX / flashing)
- `stedgeai` (for model compilation)

Set these environment variables:

- `STM32CLT_PATH`
- `STEDGEAI_CORE_DIR`

### Commands

All commands run from the repository root via `project.py`:

```bash
python project.py clean                    # Remove build artifacts
python project.py model                  # Generate ATON NPU sources + HEX for one model (see below)
python project.py model --all            # Regenerate every model (clears Networks/Src and Networks/Bin first)
python project.py proto                  # Generate protobuf outputs (nanopb C + Python modules)
python project.py proto --proto Appli/Proto/foo.proto   # Optional: single .proto file
python project.py build                  # CMake configure + compile (FSBL + Appli, Release)
python project.py build --flash          # Same, then sign trusted HEX + flash FSBL, weights, Appli (SWD)
python project.py build --debug          # Debug build (Appli + FSBL unless scoped with --appli / --fsbl)
python project.py build --debug --appli  # Appli only, Debug
python project.py build --debug --fsbl   # FSBL only, Debug
python project.py build --tracex         # Enable ThreadX TraceX hooks in Appli (use with tracex / dump tools)
python project.py format                 # clang-format C/C++ under Appli/ and FSBL/; format scripts/*.py + project.py
python project.py ui                     # Host visualizer (auto-detect firmware serial port)
python project.py ui /dev/ttyACM0        # Visualizer with explicit port
python project.py tracex                 # Request TraceX buffer over serial (needs Appli built with --tracex)
python project.py tracex-parse           # Parse tracex_dump.bin and print thread/event hotspots
```

**Model selection**

- `python project.py model` uses `--name` / `-n` for the model key (default: same as build default below).
- `python project.py build` uses `--model` / `-m` for the model baked into Appli (default: `tinyv8_320`).

| Key | Model | Notes |
|-----|--------|--------|
| `yolox_480` | YOLOX Nano 480×480 | Person |
| `yolod_256` | ST YOLODv2 Milli 256×256 | Person |
| `yolo26_320` | YOLO26 320×320 | COCO-oriented person detector |
| `tinyv8_320` | Tinyissimo YOLO v8 320×320 | Person (default) |

**`build` options**

- `--fps N` — camera frame rate; must be one of **15, 20, 25, 30** (default: 15).
- `--snapshot` — NN pipe uses single-frame camera snapshots instead of continuous capture.
- `--mode {underdrive,nominal,overdrive}` — power/clock profile (default: **underdrive**).
- `--force` / `-f` — re-sign even when the ELF hash matches the sign cache (ignored if not signing).
- `--debug` — Debug CMake preset (no trusted HEX path used for everyday iteration).
- `--appli` / `--fsbl` — build only that image; omit both to build both.
- `--flash` — after a successful Release build, run signing (`STM32_SigningTool_CLI`) and `STM32_Programmer_CLI` for FSBL, network weights, and Appli.
- `--tracex` — set `TRACEX_ENABLE=ON` in the Appli CMake configure step.

Signing and `*-trusted.hex` generation run **only** when `--flash` is passed (Release). A plain `build` compiles to ELF under each project’s `build/<preset>/` without invoking the signing tools.

### Flash memory map

| Component | Address |
|-----------|---------|
| FSBL | `0x70000000` |
| Appli | `0x70100000` |
| Network weights | `0x70380000` |

## Receiver visualizer

Host tool for length-prefixed protobuf on UART (`uint32` little-endian length + payload). Message types are `DeviceMessage` / `HostMessage` in `Appli/Proto/messages.proto`.

### Features

- Timing trends: inference, postprocess, tracker, NN period; frame drops; optional CPU usage.
- Detections with class labels from `DeviceInfo`, plus **tracks** when the firmware sends `TrackedBox` data.
- VL53L5CX **8×8** depth, per-cell sigma and signal grids, ToF period, and person-distance **alert** / stale flags.
- Optional **second serial** link to the fyp power-measure firmware (infer/idle power, energy, battery estimate) when hardware is connected.
- Host → device: `GetDeviceInfo`, `SetDisplayEnabled`, `SetPostprocessConfig`, `GetTraceXDump` (TraceX only useful if Appli was built with `--tracex`).

### Setup

From the repository root:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python project.py proto   # once, so generated *_pb2.py exists for the visualizer
```

### Run

```bash
python project.py ui --baud 921600
```

`project.py ui` forwards `--baud` and `--timeout` (serial read timeout in seconds, default 2.0) to the visualizer. See full flags with:

```bash
python project.py ui --help
```
