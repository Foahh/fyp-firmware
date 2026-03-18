# =============================================================================
# AI NPU Library
# =============================================================================
set(AI_NPU_SOURCES
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ecloader.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_cipher.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_dbgtrc.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_debug.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_lib.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_lib_sw_operators.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_osal_threadx.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_profiler.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_reloc_callbacks.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_reloc_network.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_rt_main.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_runtime.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_stai_internal.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_aton_util.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_sw_float.c
    ${LIBRARY_ROOT}/AI/Npu/ll_aton/ll_sw_integer.c
    ${LIBRARY_ROOT}/AI/Npu/Devices/STM32N6XX/mcu_cache.c
    ${LIBRARY_ROOT}/AI/Npu/Devices/STM32N6XX/npu_cache.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${LIBRARY_ROOT}/AI/Inc
            ${LIBRARY_ROOT}/AI/Npu/ll_aton
            ${LIBRARY_ROOT}/AI/Npu/Devices/STM32N6xx)

add_library(AI_NPU STATIC ${AI_NPU_SOURCES})
target_link_libraries(AI_NPU PUBLIC stm32_interface threadx STM32_Drivers)

# TODO: Handle warnings from the AI NPU library
target_compile_options(AI_NPU PRIVATE -w)
