cmake_minimum_required(VERSION 3.22)

# =============================================================================
# STM32 MCU Configuration
# =============================================================================
set(STM32_MCU_FLAGS "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse")
set(STM32_LINKER_SCRIPT "STM32N657XX_AXISRAM2_fsbl.ld")
set(STM32_LINKER_OPTION "")

# =============================================================================
# Project Definition
# =============================================================================
project(
  Firmware_FSBL
  VERSION 1.0.0
  LANGUAGES C ASM)

# =============================================================================
# Language Configuration
# =============================================================================
enable_language(C ASM)

# =============================================================================
# Compiler and Build Configuration
# =============================================================================
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      "Debug"
      CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Enable compile commands for IDE indexing (e.g., clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force C++ compiler for cross-compilation compatibility
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# =============================================================================
# Root Directory Reference
# =============================================================================
get_filename_component(FIRMWARE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LIBRARY_ROOT "${FIRMWARE_ROOT}/Libraries")

# =============================================================================
# Source Files Configuration
# =============================================================================
# Application source files
set(APPLICATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/extmem_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscall.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/startup_stm32n657xx_fsbl.s)

# STM32 HAL/LL Driver source files
set(STM32_DRIVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32n6xx_fsbl.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_bsec.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cortex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_gpio.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_exti.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_xspi.c
)

# STM32 External Memory Manager source files
set(STM32_EXTMEM_SOURCES
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/stm32_extmem.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/boot/stm32_boot_lrun.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/sal/stm32_sal_xspi.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/sal/stm32_sal_sd.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/nor_sfdp/stm32_sfdp_data.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/nor_sfdp/stm32_sfdp_driver.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/psram/stm32_psram_driver.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/sdcard/stm32_sdcard_driver.c
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/user/stm32_user_driver.c
)

# BSP source files
set(BSP_SOURCES
    ${LIBRARY_ROOT}/BSP/STM32N6570-DK/stm32n6570_discovery.c
)

# =============================================================================
# Include Directories Configuration
# =============================================================================
set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Inc
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Inc/Legacy
    ${LIBRARY_ROOT}/CMSIS/Device/ST/STM32N6xx/Include
    ${LIBRARY_ROOT}/CMSIS/Include
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/boot
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/sal
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/nor_sfdp
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/psram
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/sdcard
    ${LIBRARY_ROOT}/STM32_ExtMem_Manager/user
    ${LIBRARY_ROOT}/BSP/STM32N6570-DK)

# =============================================================================
# Compile Definitions
# =============================================================================
set(COMPILE_DEFINITIONS USE_HAL_DRIVER STM32N657xx $<$<CONFIG:Debug>:DEBUG>)

# =============================================================================
# Interface Library for Common Configuration
# =============================================================================
add_library(stm32_interface INTERFACE)
target_include_directories(stm32_interface INTERFACE ${INCLUDE_DIRECTORIES})
target_compile_definitions(stm32_interface INTERFACE ${COMPILE_DEFINITIONS})

# =============================================================================
# STM32 Driver Library
# =============================================================================
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_DRIVER_SOURCES})
target_link_libraries(STM32_Drivers PUBLIC stm32_interface)

# =============================================================================
# STM32 External Memory Manager Library
# =============================================================================
add_library(STM32_ExtMem_Manager OBJECT)
target_sources(STM32_ExtMem_Manager PRIVATE ${STM32_EXTMEM_SOURCES})
target_link_libraries(STM32_ExtMem_Manager PUBLIC stm32_interface)

# =============================================================================
# Main Executable
# =============================================================================
add_executable(${PROJECT_NAME})

# Application sources
target_sources(${PROJECT_NAME} PRIVATE ${APPLICATION_SOURCES})

# BSP sources
target_sources(${PROJECT_NAME} PRIVATE ${BSP_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRECTORIES})

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFINITIONS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE STM32_Drivers
                                              STM32_ExtMem_Manager)

# =============================================================================
# Build Configuration
# =============================================================================
# Add map file to clean target
set_target_properties(${PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES
                                                 ${PROJECT_NAME}.map)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
