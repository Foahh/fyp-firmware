# CLAUDE.md

This file provides guidance to agent when working with code in this repository.

## Project Overview

Embedded AI vision firmware for the STM32N6570-DK (Cortex-M55, ARMv8.1-M). Captures video from an IMX335 camera, runs real-time person detection via YOLO-X Nano on the ATON NPU, and displays results on an LCD. Runs on ThreadX RTOS.

## Build Commands

All commands go through `project.py` at the project root. Requires `STM32CubeCLT` installed and `STM32CLT_PATH` set, plus `cmake`, `ninja`, `arm-none-eabi-*`, `STM32_SigningTool_CLI`, `STM32_Programmer_CLI`, and `stedgeai` on PATH.

```bash
python project.py clean          # Remove build artifacts
python project.py model          # Generate ATON NPU sources + HEX from TFLite model
python project.py build          # CMake configure + compile + sign + HEX (both FSBL and Appli)
python project.py flash          # Flash FSBL, network weights, and Appli via SWD
python project.py format         # clang-format all .c/.h/.cpp/.hpp in Appli/ and FSBL/
```

`model` and `build` accept `--model / -m` to select a model (default: `yolox_nano`). Note: `project.py` uses `gen` as the subcommand name internally, but README documents it as `model`.

## Formatting

Uses `.clang-format` config: LLVM base, 2-space indent, no column limit, braces always inserted, short functions/ifs/loops never on single line.

## Architecture

Two-stage boot: **FSBL** (First Stage Boot Loader) initializes external memory and hands off to **Appli** (main application). Both are built as separate CMake ExternalProjects from the root `CMakeLists.txt` using the `gcc-arm-none-eabi` toolchain with Ninja.

### Flash Memory Map

| Component | Address |
|---|---|
| FSBL | `0x70000000` |
| Appli | `0x70100000` |
| Network weights | `0x70380000` |

### Appli Structure

- `Core/` — HAL init, startup assembly, interrupt handlers, `main.c` entry point
- `RTOS/` — ThreadX integration (`app_azure_rtos.c`, `app_threadx.c`)
- `App/` — Application modules:
  - `app_cam.c` — Camera capture pipeline
  - `app_nn.c` — Neural network inference thread (ThreadX, priority 6)
  - `app_pp.c` — YOLO-X NMS post-processing (confidence 0.6, IoU 0.5)
  - `app_lcd.c` — LCD display rendering
  - `app_ui.c` — UI logic
  - `app_bqueue.c` — Buffer queue management
  - `app_cpuload.c` — CPU load monitoring
- `cmake/` — Per-library CMake include files (10 files for each dependency)

### Key Libraries (in `Libraries/`)

- **STM32N6xx_HAL_Driver** — Hardware abstraction
- **ThreadX** — RTOS (Cortex-M55 secure port)
- **ll_aton** — ATON NPU runtime for neural network execution
- **stm32-mw-camera / stm32-mw-isp** — Camera middleware and ISP (IMX335 sensor)
- **lib_vision_models_pp / ai-postprocessing-wrapper** — YOLO-X post-processing
- **BSP** — Board support package
- **vl53l5cx** — Time-of-Flight sensor driver

### Build Configuration

- C standard: C23
- Toolchain: `gcc-arm-none-eabi` with `-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse`
- CMake presets in `CMakePresets.json` for Debug/Release
- Model selection passed as compile define (`MODEL_YOLOX_NANO`) via `-DMODEL_DEFINE` from `build.py`
- NPU configured for async mode with ThreadX OSAL (`LL_ATON_OSAL_THREADX`, `LL_ATON_RT_ASYNC`)

### Networks

Model config lives in `Networks/my_neural_art.json`. The `stedgeai` tool compiles TFLite models (from `models/`) into ATON NPU C sources placed in `Networks/` and a flashable HEX binary.
