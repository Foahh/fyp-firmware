# FYP Firmware

Embedded AI vision firmware for the STM32N6570-DK (Cortex-M55, ARMv8.1-M). Captures video from an IMX335 camera, runs real-time object detection on the NPU, and displays results on an LCD. Runs on ThreadX RTOS.

## Build

### Prerequisites

Install [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) and [STEdgeAI-Core](https://www.st.com/en/development-tools/stedgeai-core.html).

Ensure the following tools are available:

- `cmake`, `ninja`
- `arm-none-eabi-*`
- `STM32_SigningTool_CLI`, `STM32_Programmer_CLI`
- `stedgeai` (for model compilation)

Set these environment variables:

- `STM32CLT_PATH`
- `STEDGEAI_CORE_DIR`

### Commands

All commands run through `project.py` at the repository root:

```bash
python project.py clean                # Remove build artifacts
python project.py model                # Generate ATON NPU sources + HEX from TFLite model
python project.py proto                # Generate protobuf outputs (nanopb C + Python modules)
python project.py build                # CMake configure + compile + sign + HEX (both FSBL and Appli)
python project.py build --debug        # Build both in Debug mode (no sign/HEX)
python project.py build --debug --appli # Build Appli only (Debug, no sign/HEX)
python project.py build --debug --fsbl  # Build FSBL only (Debug, no sign/HEX)
python project.py flash                # Flash FSBL, network weights, and Appli via SWD
python project.py format               # clang-format all .c/.h/.cpp/.hpp in Appli/ and FSBL/
python project.py ui                    # Launch visualizer (auto-detect serial port)
python project.py ui /dev/ttyACM0      # Launch visualizer with explicit serial port
```

`model` and `build` accept `--name` / `-n` to select the model (default: `yolox_480`):

| Key | Model | Classes |
|---|---|---|
| `yolox_480` | YOLOX Nano 480×480 | person |
| `yolod_256` | ST YOLODv2 Milli 256×256 | person |
| `yolo26_320` | YOLO26 320×320 (COCO) | person |
| `yolo26_320_fyp` | YOLO26 320×320 (finetuned) | hand, tool |

`build` also accepts:

- `--fps N` (camera frame rate, default 30)
- `--snapshot` (NN pipe uses single-frame camera snapshots instead of continuous capture)
- `--mode {underdrive,nominal,overdrive}` (clock/voltage profile, default: nominal)
- `--force` (re-sign even if unchanged)
- `--debug` (Debug mode, no sign/HEX)
- `--appli` (build Appli only)
- `--fsbl` (build FSBL only)

Without `--appli`/`--fsbl`, both images are built.

### Flash Memory Map

| Component | Address |
|---|---|
| FSBL | `0x70000000` |
| Appli | `0x70100000` |
| Network weights | `0x70380000` |

## Receiver Visualizer

Real-time host visualizer for `Appli/Serial` messages (`DeviceMessage` / `HostMessage` in `Appli/Proto/messages.proto`).

### Features

- Reads length-prefixed protobuf frames from UART (`<uint32_le length> + payload`)
- Displays timing trends (inference, postprocess, NN period)
- Shows ToF 8x8 depth heatmap and alert status
- Lists detections with class labels and confidence
- Sends host commands:
  - `GetDeviceInfo`
  - `SetDisplayEnabled` (toggle)

### Setup

From repository root:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

### Run

```bash
python project.py ui --baud 921600
```

Use `--help` for all options:

```bash
python project.py ui --help
```