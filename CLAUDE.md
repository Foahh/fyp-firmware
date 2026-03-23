# CLAUDE.md

This file provides guidance to agent when working with code in this repository.

## Project Overview

Embedded AI vision firmware for the STM32N6570-DK (Cortex-M55, ARMv8.1-M). Captures video from an IMX335 camera, runs real-time object detection on the NPU, and displays results on an LCD. Runs on ThreadX RTOS.

## Build Commands

All commands go through `project.py` at the project root. Requires `STM32CubeCLT` installed and `STM32CLT_PATH` set, plus `cmake`, `ninja`, `arm-none-eabi-*`, `STM32_SigningTool_CLI`, `STM32_Programmer_CLI`, and `stedgeai` on PATH.

```bash
python project.py clean                # Remove build artifacts
python project.py model                # Generate ATON NPU sources + HEX from TFLite model
python project.py proto                # Generate protobuf outputs (nanopb C + Python modules)
python project.py build                       # CMake configure + compile + sign + HEX (both FSBL and Appli)
python project.py build --debug               # Build both in Debug mode (no sign/HEX)
python project.py build --debug --appli       # Build Appli only (Debug, no sign/HEX)
python project.py build --debug --fsbl        # Build FSBL only (Debug, no sign/HEX)
python project.py flash                       # Flash FSBL, network weights, and Appli via SWD
python project.py format                      # clang-format all .c/.h/.cpp/.hpp in Appli/ and FSBL/
```

`model` and `build` accept `--name / -n` to select a model (default: `yolox_nano`). `build` also accepts `--snapshot` (snapshot camera mode), `--performance` (NPU at 1000 MHz vs 800 MHz), `--fps N` (camera frame rate, default 30), `--force` (re-sign even if unchanged), `--debug` (Debug mode, no sign/HEX), `--appli` (build Appli only), and `--fsbl` (build FSBL only). Default without `--appli`/`--fsbl` builds both.

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

- `Core/` — HAL init, startup assembly, interrupt handlers, RTOS init, `main.c` entry point
  - `main.c` — Entry point, HAL callbacks (`HAL_TIM_PeriodElapsedCallback`, `HAL_MspInit`, `BSP_PB_Callback`)
  - `init_clock.c` — `SystemClock_Config()`, `ClockSleep_Config()`
  - `init_peripherals.c` — GPIO, SMPS, IAC, XSPI, LED, Button, COM, NPU, Priority, SystemIsolation configs
  - `init_mpu.c` — MPU region configuration
  - `app_azure_rtos.c`, `app_threadx.c` — ThreadX integration
- `Camera/` — Camera capture pipeline
  - `Src/cam.c` — ISP thread, LCD reload thread, thread lifecycle
  - `Src/cam_init.c` — `CAM_Init`, pipe config, ROI calculation, DeInit
  - `Src/cam_pipe.c` — Pipe start/stop/resume, frame callbacks, buffer management
- `NN/` — Neural network
  - `Src/nn_thread.c` — NN inference thread (ThreadX, priority 6)
  - `Src/pp_thread.c` — YOLO-X NMS post-processing (confidence 0.6, IoU 0.5)
- `Display/` — Display and UI
  - `Src/lcd.c` — LTDC dual-layer display (Layer 0: camera, Layer 1: UI overlay)
  - `Src/ui.c` — UI thread entry, public API, buffer swap
  - `Src/ui_panel.c` — Diagnostics panel rendering
  - `Src/ui_overlay.c` — Detection bounding boxes, ROI rectangle
  - `Src/ui_depth.c` — ToF depth grid heatmap, proximity alert banner
- `Serial/` — Host communication (protobuf over UART)
  - `Src/comm_cmd.c` — Command dispatcher
  - `Src/comm_log.c` — Periodic device-to-host reporting
  - `Src/comm_rx.c` — UART RX with ring buffer
  - `Src/comm_tx.c` — TX encoding with double-buffered frames
- `Sensor/` — External sensors
  - `Src/tof.c` — VL53L5CX 8x8 depth ranging, hand/hazard proximity fusion
  - `Src/haptic.c` — Haptic motor GPIO control
  - `Src/power_measurement_sync.c` — Power measurement synchronization
- `Util/` — Utilities
  - `Src/bqueue.c` — SPSC buffer queue (semaphore-based)
  - `Src/cpuload.c` — CPU load measurement via DWT cycle counter
- `Config/Inc/` — Build-time configuration (`cam_config.h`, `lcd_config.h`, `nn_config.h`, `app_config.h`, `model_config.h`)
  - `models/` — Per-model constant headers (`model_yolox_nano.h`)
- `CMake/` — Per-library CMake include files (one per dependency)

### Naming Conventions

- Public functions: `Module_VerbNoun()` (e.g., `CAM_ThreadStart()`, `UI_ThreadSuspend()`, `PP_GetInfo()`)
- Thread start functions take no parameters (no unused `VOID *memory_ptr`)
- Headers: no `app_` prefix (except `app_lcd.h` to avoid collision with ST's `lcd.h`), named after module (e.g., `cam.h`, `nn.h`, `pp.h`)
- Internal headers: `*_internal.h` (e.g., `ui_internal.h`, `cam_internal.h`) for cross-file shared state within a subsystem

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
