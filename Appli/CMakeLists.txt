cmake_minimum_required(VERSION 3.22)

# =============================================================================
# STM32 MCU Configuration
# =============================================================================
set(STM32_MCU_FLAGS "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse")
set(STM32_LINKER_SCRIPT "Startup/STM32N657XX_LRUN.gen.ld")
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
    ${CMAKE_CURRENT_SOURCE_DIR}/Startup/startup_stm32n657xx.s
    ${CMAKE_CURRENT_SOURCE_DIR}/Startup/tx_initialize_low_level.S
    ${CMAKE_CURRENT_SOURCE_DIR}/Util/Src/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/init_clock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/init_mpu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/init_peripherals.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_azure_rtos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_threadx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/secure_nsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c)

# App source files
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera/Src/cam.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera/Src/cam_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera/Src/cam_pipe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NN/Src/nn_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NN/Src/pp_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Src/display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Src/ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Src/ui_panel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Src/ui_overlay.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Src/ui_depth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Serial/Src/comm_cmd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Serial/Src/comm_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Serial/Src/comm_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Serial/Src/comm_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Sensor/Src/tof.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Sensor/Src/tof_fusion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Sensor/Src/power_measurement_sync.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Util/Src/cpu_load.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Util/Src/tracex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Util/Src/bqueue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Util/Src/timebase.c)

# =============================================================================
# Include Directories Configuration
# =============================================================================
set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Serial/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/NN/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Display/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Sensor/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Util/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Config/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Proto
    ${EXTERNAL_ROOT}/nanopb
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc)

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
# Platform/library definitions
set(PLATFORM_DEFINITIONS
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

option(TRACEX_ENABLE "Enable ThreadX TraceX event trace collection" OFF)
if(TRACEX_ENABLE)
  list(APPEND PLATFORM_DEFINITIONS TRACEX_ENABLE)
endif()

# App-only definitions
set(APP_DEFINITIONS "")

option(SNAPSHOT_MODE "NN pipe: single-frame snapshots instead of continuous capture" OFF)
if(SNAPSHOT_MODE)
  list(APPEND APP_DEFINITIONS SNAPSHOT_MODE)
endif()

set(POWER_MODE
    "nominal"
    CACHE STRING "Power/clock mode: underdrive (all@400MHz), nominal (CPU@600/NPU@800), overdrive (CPU@800/NPU@1000)")
set_property(CACHE POWER_MODE PROPERTY STRINGS underdrive nominal overdrive)
string(TOUPPER "${POWER_MODE}" _POWER_MODE_UPPER)
if(_POWER_MODE_UPPER STREQUAL "UNDERDRIVE")
  list(APPEND APP_DEFINITIONS "POWER_MODE=0")
elseif(_POWER_MODE_UPPER STREQUAL "NOMINAL")
  list(APPEND APP_DEFINITIONS "POWER_MODE=1")
elseif(_POWER_MODE_UPPER STREQUAL "OVERDRIVE")
  list(APPEND APP_DEFINITIONS "POWER_MODE=2")
else()
  message(FATAL_ERROR "POWER_MODE must be underdrive, nominal, or overdrive (got '${POWER_MODE}')")
endif()

set(CAMERA_FPS_ALLOWED 10 15 20 25 30)
set(CAMERA_FPS
    15
    CACHE STRING "Camera frame rate (Hz); sensor supports 10/15/20/25/30 only")
set_property(CACHE CAMERA_FPS PROPERTY STRINGS ${CAMERA_FPS_ALLOWED})
if(NOT CAMERA_FPS IN_LIST CAMERA_FPS_ALLOWED)
  string(REPLACE ";" " " _CAMERA_FPS_LIST "${CAMERA_FPS_ALLOWED}")
  message(
    FATAL_ERROR
      "CAMERA_FPS must be one of: ${_CAMERA_FPS_LIST}, got '${CAMERA_FPS}'")
endif()
list(APPEND APP_DEFINITIONS "CAMERA_FPS=${CAMERA_FPS}")

# =============================================================================
# Interface Library for Common Configuration
# =============================================================================
add_library(stm32_interface INTERFACE)
target_include_directories(stm32_interface INTERFACE ${INCLUDE_DIRECTORIES})
target_compile_definitions(stm32_interface INTERFACE ${PLATFORM_DEFINITIONS})

# =============================================================================
# Include Library CMake Files
# =============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/stm32_drivers.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/threadx.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/cmw_camera.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/isp_library.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/bsp.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/lcd.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/ai_postprocessing.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/ai_npu.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/vl53l5cx.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/network.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/tracker.cmake)

include(${CMAKE_CURRENT_SOURCE_DIR}/CMake/nanopb.cmake)

# =============================================================================
# Build Timestamp (regenerated on every build)
# =============================================================================
set(BUILD_TIMESTAMP_H "${CMAKE_CURRENT_BINARY_DIR}/build_timestamp.h")
add_custom_target(
  build_timestamp ALL
  COMMAND ${CMAKE_COMMAND} -P
          "${CMAKE_CURRENT_SOURCE_DIR}/CMake/build_timestamp.cmake"
  BYPRODUCTS ${BUILD_TIMESTAMP_H}
  COMMENT "Generating build timestamp")

# =============================================================================
# Main Executable
# =============================================================================
add_executable(${PROJECT_NAME})

# Application sources
target_sources(${PROJECT_NAME} PRIVATE ${CORE_SOURCES} ${APP_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE stm32_interface)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${APP_DEFINITIONS})
add_dependencies(${PROJECT_NAME} build_timestamp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

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
          Tracker
          Nanopb
          m
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
