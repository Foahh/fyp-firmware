# CLAUDE.md

This file provides guidance to agents when working with code in this repository.

## Project Overview

Embedded AI camera firmware for the STM32N6570-DK development kit. Runs real-time YOLO-X Nano person detection on a Cortex-M55 MCU with ATON NPU, displaying results on a 5-inch LCD with camera feed overlay.

- MCU: STM32N657xx (Cortex-M55, 480 MHz)
- RTOS: Azure ThreadX
- AI Model: YOLO-X Nano (480x480, INT8 quantized)
- Camera: IMX335 via DCMIPP dual-pipe
- Display: RK050HR18 LCD (480x272, RGB565)

## Build Commands

All commands use `build.py` at the project root:

```bash
python build.py clean      # Remove build artifacts
python build.py gen        # Generate network model sources/binaries (requires stedgeai)
python build.py build      # CMake configure + compile + sign + convert to HEX
python build.py flash      # Flash FSBL, network model, and Appli to device via SWD
```

Build always targets Release. The pipeline is: CMake/Ninja → ELF → binary → STM32_SigningTool_CLI → HEX.

### Required Tools (all must be on PATH)

- `cmake`, `ninja`
- `arm-none-eabi-gcc` / `arm-none-eabi-g++` / `arm-none-eabi-objcopy`
- `STM32_SigningTool_CLI`, `STM32_Programmer_CLI`
- `stedgeai`
- Environment variable `STM32CLT_PATH`

## Architecture

### Two-Stage Boot

The firmware is split into two independently built CMake projects:

1. **FSBL** (`FSBL/`) — First Stage Boot Loader. Minimal startup code, initializes clocks and external memory, then jumps to Appli. Linked to AXISRAM2 (~2KB stack).
2. **Appli** (`Appli/`) — Main application. Contains all runtime logic, ThreadX RTOS, camera pipeline, NN inference, and UI.

Each has its own `CMakeLists.txt`, `CMakePresets.json`, and linker script. The root `CMakeLists.txt` builds both via `ExternalProject`.

### Flash Memory Map

| Component | Address |
|-----------|---------|
| FSBL | `0x70000000` |
| Appli | `0x70100000` |
| Network weights | `0x70380000` |

### Application Threading (Appli/RTOS/)

ThreadX threads with cooperative priorities:

- **ISP thread** (priority 5): Camera image signal processing updates
- **NN thread** (priority 6): Runs YOLO-X inference on ATON NPU
- **UI thread**: Renders bounding boxes and overlays at 30 FPS

### Data Pipeline

Camera (IMX335) → DCMIPP dual pipes → ISP → NN inference (ATON NPU) → Post-processing (NMS) → LCD overlay

DCMIPP Pipe 1 feeds the display (480x272 RGB565), Pipe 2 feeds the NN (480x480 cropped).

### Key Application Modules (Appli/App/Src/)

- `app_cam.c` — Camera/ISP init, frame event handling
- `app_nn.c` — ATON runtime setup, network loading, inference loop
- `app_postprocess.c` — YOLO-X post-processing (NMS, confidence thresholding)
- `app_ui.c` — LCD overlay rendering, detection visualization
- `app_lcd.c` — LCD driver integration
- `app_buffers.c` / `app_bqueue.c` — Frame buffer memory and queue management

### NN Configuration

- 1 class (person), 3 anchors, multi-scale grids (60x60, 30x30, 15x15)
- Confidence threshold: 0.6, NMS IoU threshold: 0.5, max 10 detections
- Model file: `Networks/models/st_yolo_x_nano_480_1.0_0.25_3_int8.tflite`
- Generated sources go to `Networks/Src/`, binaries to `Networks/Bin/`

### Libraries (Libraries/)

All vendored. Key ones:
- `AI/` — STEdgeAI runtime (ATON NPU driver + ll_aton)
- `BSP/` — Board Support Package for STM32N6570-DK
- `STM32N6xx_HAL_Driver/` — STM32 HAL
- `threadx/` — Azure ThreadX RTOS
- `stm32-mw-camera/` / `stm32-mw-isp/` — Camera and ISP middleware
- `lib_vision_models_pp/` — Vision model post-processing
