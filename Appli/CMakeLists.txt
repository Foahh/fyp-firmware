cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse" )

#Linker options
set(STM32_LINKER_SCRIPT STM32N657XX_LRUN.ld)
set(STM32_LINKER_OPTION  )

# Set the project name
set(CMAKE_PROJECT_NAME Firmware_Appli)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")

# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")


# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # AI Library platform configuration
	LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6
	LL_ATON_OSAL=LL_ATON_OSAL_THREADX
	LL_ATON_RT_MODE=LL_ATON_RT_ASYNC
    APP_HAS_PARALLEL_NETWORKS=0
    LL_ATON_SW_FALLBACK
    $<$<CONFIG:Debug>:LL_ATON_DUMP_DEBUG_API>
	$<$<CONFIG:Debug>:LL_ATON_EB_DBG_INFO>
    LL_ATON_DBG_BUFFER_INFO_EXCLUDED=1
)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6570-DK
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/Components/Common
    # CMSIS includes
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/CMSIS/DSP/Include
    # Camera Middleware includes
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/sensors/imx335
    # ISP Library includes
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/STM32_ISP_Library/isp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/STM32_ISP_Library/evision/Inc
    # Libraries includes
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lcd
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/Fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/ai-postprocessing-wrapper/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lock-free-ring-buffer/src
    # AI Library includes
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6xx
)

# Camera Middleware sources
set(CMW_CAMERA_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/cmw_camera.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/cmw_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/sensors/cmw_imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/sensors/imx335/imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/stm32-mw-camera/sensors/imx335/imx335_reg.c
)

# ISP Library sources
set(ISP_LIBRARY_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/STM32_ISP_Library/isp/Src/isp_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/STM32_ISP_Library/isp/Src/isp_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/STM32_ISP_Library/isp/Src/isp_services.c
)

# BSP Driver sources
set(BSP_Driver_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6570-DK/stm32n6570_discovery.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_bus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/Components/aps256xx/aps256xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Drivers/BSP/Components/mx66uw1g45g/mx66uw1g45g.c
)

set(LIBRARIES_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lcd/stm32_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lock-free-ring-buffer/src/ring_buf.c
)

set(AI_POSTPROCESSING_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/ai-postprocessing-wrapper/app_postprocess_od_st_yolox_uf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/od_pp_st_yolox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp.c
)

# AI NPU Library sources
set(AI_NPU_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ecloader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_cipher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_dbgtrc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_lib_sw_operators.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_osal_threadx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_profiler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_reloc_callbacks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_reloc_network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_rt_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_sw_float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_sw_integer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6XX/mcu_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6XX/npu_cache.c
)

set(APP_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_buffers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_cam.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_event_bus.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${STM32_HAL_Driver_Src}
    ${BSP_Driver_Src}
    ${APP_Src}
    ${CMW_CAMERA_Src}
    ${ISP_LIBRARY_Src}
    ${LIBRARIES_Src}
    ${AI_POSTPROCESSING_Src}
    ${AI_NPU_Src}
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
    ${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/STM32_ISP_Library/evision/Lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Lib
)

# Add linked libraries (using plain signature to match mx-generated.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME}
    # Add user defined libraries
    # ISP evision static libraries (Auto Exposure & Auto White Balance)
    :libn6-evision-st-ae_gcc.a
    :libn6-evision-awb_gcc.a
    # AI Network Runtime library
    :NetworkRuntime1100_CM55_GCC.a
    # Math library required by evision libraries
    m
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()





