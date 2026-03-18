# FYP

Targeted platform: *STM32N6570-DK*

## Build

### Prerequisites

Install [STM32CubeCLT](https://www.st.com/en/development-tools/stm32cubeclt.html) and [STEdgeAI-Core](https://www.st.com/en/development-tools/stedgeai-core.html).

Ensure the following tools are on your PATH:

- `cmake`, `ninja`
- `arm-none-eabi-*`,
- `STM32_SigningTool_CLI`, `STM32_Programmer_CLI`
- `stedgeai` (for network model generation)

The environment variable `STM32CLT_PATH` must also be set.

### Commands

All commands are run via `build.py` at the project root:

```bash
python build.py clean      # Remove build artifacts
python build.py model      # Generate network model sources and binaries
python build.py build      # CMake configure + compile + sign + convert to HEX
python build.py flash      # Flash FSBL, network model, and Appli to device via SWD
```

`model` and `build` accept a `--model` / `-m` flag to select the model (default: `yolox_nano`).

### Build Pipeline

1. `model` — Runs `stedgeai` to compile the TFLite model into ATON NPU sources and a flashable network binary (HEX).
2. `build` — Configures and compiles both FSBL and Appli via CMake/Ninja, then converts each ELF → binary → signed binary → HEX.
3. `flash` — Programs FSBL, network weights, and Appli to external flash over SWD using `STM32_Programmer_CLI`.

### Flash Memory Map

| Component | Address |
|---|---|
| FSBL | `0x70000000` |
| Appli | `0x70100000` |
| Network weights | `0x70380000` |