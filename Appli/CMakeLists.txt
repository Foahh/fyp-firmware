cmake_minimum_required(VERSION 3.22)

# =============================================================================
# STM32 MCU Configuration
# =============================================================================
set(STM32_MCU_FLAGS "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse")
set(STM32_LINKER_SCRIPT "STM32N657XX_LRUN.ld")
set(STM32_LINKER_OPTION "")

# =============================================================================
# Project Definition
# =============================================================================
project(
  Firmware_Appli
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

# Enable compile commands for IDE indexing
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force C++ compiler for cross-compilation compatibility
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# =============================================================================
# Root Directory Reference
# =============================================================================
get_filename_component(FIRMWARE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(LIBRARY_ROOT "${FIRMWARE_ROOT}/Libraries")
set(EXTERNAL_ROOT "${FIRMWARE_ROOT}/External")
set(STM32CUBE_ROOT "${EXTERNAL_ROOT}/STM32CubeN6")

# =============================================================================
# Source Files Configuration
# =============================================================================
# Core source files
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/startup_stm32n657xx.s
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/tx_initialize_low_level.S
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/power_measurement_sync.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/secure_nsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c)

# RTOS source files
set(RTOS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/app_azure_rtos.c
                 ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/app_threadx.c)

# App source files
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_bqueue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_cam.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_comm_cmd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_comm_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_comm_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_comm_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_cpuload.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_nn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_pp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_tof.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_ui.c)

# =============================================================================
# Include Directories Configuration
# =============================================================================
set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc)

# =============================================================================
# Model Selection
# =============================================================================
if(NOT DEFINED MODEL_DEFINE)
  message(
    FATAL_ERROR
      "MODEL_DEFINE is not set. Pass -DMODEL_DEFINE=<MODEL_xxx> from build.py.")
endif()

# =============================================================================
# Compile Definitions
# =============================================================================
set(COMPILE_DEFINITIONS
    USE_HAL_DRIVER
    STM32N657xx
    TX_INCLUDE_USER_DEFINE_FILE
    TX_SINGLE_MODE_SECURE=1
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6
    LL_ATON_OSAL=LL_ATON_OSAL_THREADX
    LL_ATON_RT_MODE=LL_ATON_RT_ASYNC
    APP_HAS_PARALLEL_NETWORKS=0
    LL_ATON_SW_FALLBACK
    LL_ATON_DBG_BUFFER_INFO_EXCLUDED=1
    ${MODEL_DEFINE})

option(CAMERA_NN_SNAPSHOT_MODE "NN camera pipe snapshot mode" OFF)
if(CAMERA_NN_SNAPSHOT_MODE)
  list(APPEND COMPILE_DEFINITIONS CAMERA_NN_SNAPSHOT_MODE)
endif()

option(PERFORMANCE_MODE "NPU at 1000 MHz (PLL2) and VDDCORE SCALE0; OFF = 800 MHz / SCALE1" OFF)
if(PERFORMANCE_MODE)
  list(APPEND COMPILE_DEFINITIONS PERFORMANCE_MODE)
endif()

set(CAMERA_FPS_ALLOWED 10 15 20 25 30)
set(CAMERA_FPS
    30
    CACHE STRING "Camera frame rate (Hz); sensor supports 10/15/20/25/30 only")
set_property(CACHE CAMERA_FPS PROPERTY STRINGS ${CAMERA_FPS_ALLOWED})
if(NOT CAMERA_FPS IN_LIST CAMERA_FPS_ALLOWED)
  string(REPLACE ";" " " _CAMERA_FPS_LIST "${CAMERA_FPS_ALLOWED}")
  message(
    FATAL_ERROR
      "CAMERA_FPS must be one of: ${_CAMERA_FPS_LIST}, got '${CAMERA_FPS}'")
endif()
list(APPEND COMPILE_DEFINITIONS "CAMERA_FPS=${CAMERA_FPS}")

# =============================================================================
# Interface Library for Common Configuration
# =============================================================================
add_library(stm32_interface INTERFACE)
target_include_directories(stm32_interface INTERFACE ${INCLUDE_DIRECTORIES})
target_compile_definitions(stm32_interface INTERFACE ${COMPILE_DEFINITIONS})

# =============================================================================
# Include Library CMake Files
# =============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32_drivers.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/threadx.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmw_camera.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/isp_library.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bsp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/lcd.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ai_postprocessing.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ai_npu.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/vl53l5cx.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/network.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/nanopb.cmake)

# =============================================================================
# Main Executable
# =============================================================================
add_executable(${PROJECT_NAME})

# Application sources
target_sources(${PROJECT_NAME} PRIVATE ${CORE_SOURCES} ${APP_SOURCES}
                                       ${RTOS_SOURCES})

# Include directories and compile definitions are inherited from stm32_interface
# via linked libraries
target_link_libraries(${PROJECT_NAME} PRIVATE stm32_interface)

# Link libraries
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE STM32_Drivers
          threadx
          CMW_Camera
          ISP_Library
          BSP
          LCD
          AI_Postprocessing
          AI_NPU
          VL53L5CX
          Network
          Nanopb
          ${TOOLCHAIN_LINK_LIBRARIES})

# Link options
target_link_options(${PROJECT_NAME} PRIVATE -u _printf_float)

# =============================================================================
# Build Configuration
# =============================================================================
# Add map file to clean target
set_target_properties(${PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES
                                                 ${PROJECT_NAME}.map)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)
