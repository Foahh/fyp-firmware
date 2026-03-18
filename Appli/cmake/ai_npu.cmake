# =============================================================================
# AI NPU Library
# =============================================================================
set(AI_NPU_SOURCES
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_stai_internal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_sw_float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_sw_integer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6XX/mcu_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6XX/npu_cache.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Inc
            ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton
            ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6xx)

add_library(AI_NPU STATIC ${AI_NPU_SOURCES})
target_link_libraries(AI_NPU PUBLIC stm32_interface threadx STM32_Drivers)

# TODO: Handle warnings from the AI NPU library
target_compile_options(AI_NPU PRIVATE -w)
