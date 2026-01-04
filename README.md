# FYP - Firmware

## TODOs

### In Progress

- [ ] Integrate X-CUBE-AI

### Completed

- [x] First LED blinking LRUN project
- [x] ThreadX RTOS integration
- [x] Camera middleware & single DCMIPP pipe
- [x] Debug & automated build/sign/upload process
- [x] LTDC implementation
- [x] Dual DCMIPP pipes
- [x] Camera pipeline implementation

### Cancelled

- ~~TouchGFX integration~~ (Reason: overkill)

## Bookmarks

## Debug Report

### 1. `HAL_Delay()` / `HAL_GetTick()` broken after introducing ThreadX

#### Symptom
- After enabling **ThreadX** and initializing **DCMIPP**, `HAL_GetTick()` always returned **0**.
- Any HAL code that relies on tick timing (notably `HAL_Delay()`, timeouts, polling loops) malfunctioned.

#### Root cause
- The default HAL time base (SysTick or timer-based tick) was no longer incrementing as expected once ThreadX took ownership of timing / scheduling (or SysTick configuration changed).
- HAL's tick source must be aligned to the RTOS tick source when using ThreadX.

#### Fix
Override the HAL tick functions to use ThreadX time and disable HAL's own tick init:

```c
uint32_t HAL_GetTick(void)
{
  return (tx_time_get() * 1000) / TX_TIMER_TICKS_PER_SECOND;
}

void HAL_Delay(uint32_t Delay)
{
  assert(!IS_IRQ_MODE());  // Delay must not be called from ISR context

  uint32_t ticks = (Delay * TX_TIMER_TICKS_PER_SECOND) / 1000;
  tx_thread_sleep(ticks);
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  UNUSED(TickPriority);
  return HAL_OK;
}
```

### Notes / gotchas
- `tx_thread_sleep(0)` yields only; if `Delay` can be very small, consider ensuring `ticks >= 1` when `Delay > 0`.
- If any code calls `HAL_Delay()` inside an ISR (directly or indirectly), it must be refactored.

---

### 2. RAM/ROM region overflow after configuring DCMIPP / LTDC (frame buffer size)

#### Symptom
Linker error indicating **RAM/ROM overflow**, e.g. large `.bss`/`.data` allocations failing:

![Build Failure 1](Assets/1.png)

> region xxx overflowed by ... bytes

#### Root cause
Camera/display frame buffers are large. Example:

```c
uint8_t ui_buffer[...]; 
```

This easily exceeds internal SRAM.

#### Fix: move frame buffers into external PSRAM

##### Linker script
Add PSRAM memory and a dedicated section:

```ld
MEMORY
{
  ...
  PSRAM  (xrw) : ORIGIN = 0x91000000, LENGTH = 16M
}

SECTIONS
{
  ...
  .psram_section (NOLOAD) :
  {
    . = ALIGN(32);
    *(.psram_bss)
    . = ALIGN(32);
  } >PSRAM
}
```

##### Attributes in C
```c
#define ALIGN_32 __attribute__((aligned(32)))
#define IN_PSRAM __attribute__((section(".psram_bss")))
```

##### BSP init (required)
```c
BSP_XSPI_RAM_Init(0);
BSP_XSPI_RAM_EnableMemoryMappedMode(0);
```

---

### 2.1. PSRAM init causes freezes / "undebuggable" exceptions

#### Symptom
After enabling PSRAM (init + memory-mapped mode), the MCU will:
- hard fault / lock up,
- hang so badly that the debugger becomes unstable or cannot halt reliably.

#### Root cause(s)
Common causes on STM32 with external memory:
- **MPU region missing or incorrect** for the PSRAM range (access faults).
- Security/RIF configuration prevents masters (DCMIPP/LTDC/DMA) from accessing the region.
- Cache/coherency issues.

#### Fix 1: Allow PSRAM address range in MPU
```c
MPU_InitStruct.Number = MPU_REGION_NUMBER1;
MPU_InitStruct.BaseAddress = 0x91000000;
MPU_InitStruct.LimitAddress = 0x91FFFFFF;
```

#### Fix 2: Allow DMA / peripheral masters via RIF (security configuration)
```c
static void SystemIsolation_Config(void)
{
  __HAL_RCC_RIFSC_CLK_ENABLE();

  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv  = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;

  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1,  &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2,  &RIMC_master);

  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI,    RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC,   RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}
```

---

### 3. LTDC shows black screen when running from debugger (camera pipeline started)

#### Symptom
- LTDC stays black once the camera pipeline is started.
- Layer test drawings may work, but camera content does not.
- DCMIPP reports interrupts with **overrun error**.

#### Root cause
When running under the debugger, the application is started directly and **bypasses FSBL**, so the **system/peripheral clock configuration normally done in FSBL is not applied**.  
As a result, the camera pipeline (DCMIPP) runs with incorrect clocks and can trigger **overrun** and produce no valid output to LTDC.

#### Resolution
Copy the clock configuration code from **FSBL** into the application and **enable it when running from the debugger**.

---

### 4. FSBL cannot boot into Application

#### Symptom
- XSPI2's clock is set to 200 MHz
- Device runs fine from IDE/debugger, but **FSBL does not jump to / boot** the application from flash.
- Root cause was initially hidden because direcly IDE debugging bypassed the real boot path.

#### Root cause 1: image size / header handling (LAT1587)
FSBL determines application size using the v2.3 header fields:

```c
// defined in UM3234
#define HEADER_V2_3_IMG_SIZE_OFFSET 108
#define HEADER_V2_3_SIZE 1024

uint32_t BOOT_GetApplicationSize(uint32_t img_addr)
{
  uint32_t img_size;

  img_size = (*(uint32_t *)(img_addr + HEADER_V2_3_IMG_SIZE_OFFSET));
  img_size += HEADER_V2_3_SIZE;

  return img_size;
}
```

If the header/size is wrong, FSBL may copy/jump incorrectly.

#### Root cause 2: high-speed XSPI requires correct voltage + HSLV fuse
At **200 MHz XSPI_NOR**, the board must support high-speed low-voltage IO mode.

##### Program fuse to allow HSLV (VDDIO2/VDDIO3)
```c
uint32_t fuse_id = BSEC_HW_CONFIG_ID;
uint32_t data;
uint32_t bit_mask = BSEC_HWS_HSLV_VDDIO3 | BSEC_HWS_HSLV_VDDIO2;

HAL_BSEC_OTP_Read(&handler, fuse_id, &data);
data |= bit_mask;
HAL_BSEC_OTP_Program(&handler, fuse_id, data, HAL_BSEC_NORMAL_PROG);
```

##### Ensure VDDIO3 is configured for 1.8 V range when needed
In `HAL_XSPI_MspInit()`:

```c
__HAL_RCC_PWR_CLK_ENABLE();
HAL_PWREx_EnableVddIO3();
HAL_PWREx_ConfigVddIORange(PWR_VDDIO3, PWR_VDDIO_RANGE_1V8);
```

#### Key lesson
Always validate the **real boot flow** (FSBL → application) after clock/memory changes; debugger runs can hide FSBL/header/voltage constraints.

---

## 5 Camera output shows severe tearing and flickering when booting from FSBL into Application

### Symptom
- FSBL successfully boots into the Application.
- Camera output shows **severe tearing and flickering** (unstable frames).
- Only reproducible on the **full boot path (FSBL → Application)**.
- **Not reproducible** when running the Application directly from the debugger.

### Root cause
The external memory interface clocks were effectively too aggressive for the real boot configuration:

- **XSPI1 (PSRAM)** clock was **implicitly left at a high default** (observed as **~200 MHz**) because it wasn't explicitly configured in the Application clock tree.
- **XSPI2 (NOR)** was also running at **200 MHz**.
- During the full boot sequence, these higher clocks reduced timing margin for:
  - memory-mapped PSRAM reads/writes (frame buffer traffic),
  - concurrent DMA masters (DCMIPP/LTDC) accessing external memory,
  - potential voltage/IO-level constraints at high speed.

This manifested as tearing/flickering consistent with intermittent read/write corruption or starvation in the framebuffer path.

### Unknown / open question
Why it doesn't occur when starting from the debugger.

### Fix
- Updated overall system clock configuration.
- Reduced external memory clocks to improve timing margin:
  - Set **XSPI2 (NOR)** to **100 MHz** (from 200 MHz).
  - Explicitly set **XSPI1 (PSRAM)** to **100 MHz** (was previously not configured and effectively defaulted to 200 MHz).

This stabilized camera → framebuffer → LTDC behavior when running through FSBL.

### Key lesson
If tearing/flickering/data corruption only appears on the **real boot path**, audit **peripheral clocks that were "assumed"** (XSPI/QSPI, memory-mapped RAM/flash). External memory used by camera/display pipelines is highly sensitive to clock rate, IO voltage range, MPU/cache policy, and init order—reduce clocks first to recover stability, then increase cautiously with proper voltage + timing validation.