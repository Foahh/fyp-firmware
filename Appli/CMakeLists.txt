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
# Source Files Configuration
# =============================================================================
# Application source files
set(APPLICATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/app_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/app_azure_rtos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/app_threadx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/startup_stm32n657xx.s
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Startup/tx_initialize_low_level.S
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/secure_nsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/syscalls.c)

# STM32 HAL/LL Driver source files
set(STM32_DRIVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32n6xx_s.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_xspi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dcmipp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma2d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rif.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cacheaxi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim_ex.c
)

# ThreadX middleware source files
set(THREADX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_secure_stack_initialize.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_secure_stack_allocate.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_secure_stack_free.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_context_restore.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_context_save.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_interrupt_control.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_interrupt_disable.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_interrupt_restore.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_schedule.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_stack_build.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_system_return.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_timer_interrupt.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_initialize_high_level.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_initialize_kernel_enter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_initialize_kernel_setup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/tx_thread_secure_stack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/txe_thread_secure_stack_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/src/txe_thread_secure_stack_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_stack_error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_stack_error_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_system_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_pool_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_pool_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_block_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_pool_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_byte_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_event_flags_set_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_mutex_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_flush.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_front_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_queue_send_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_ceiling_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_semaphore_put_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_entry_exit_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_identify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_preemption_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_shell_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_stack_analyze.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_system_preempt_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_system_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_terminate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_time_slice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_time_slice_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_timeout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_thread_wait_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_time_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_time_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_block_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_block_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_block_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_block_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_block_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_block_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_byte_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_byte_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_byte_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_byte_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_byte_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_event_flags_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_event_flags_set_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_mutex_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_mutex_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_mutex_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_mutex_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_flush.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_front_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_queue_send_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_ceiling_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_semaphore_put_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_entry_exit_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_preemption_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_terminate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_time_slice_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_thread_wait_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_expiration_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_system_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_system_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/tx_timer_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_timer_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_timer_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_timer_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/src/txe_timer_info_get.c
)

# Camera Middleware source files
set(CMW_CAMERA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/cmw_camera.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/cmw_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/cmw_imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/imx335/imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/imx335/imx335_reg.c
)

# ISP Library source files
set(ISP_LIBRARY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Src/isp_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Src/isp_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Src/isp_services.c
)

# BSP Driver source files
set(BSP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery_bus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ramcfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/Components/aps256xx/aps256xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/Components/mx66uw1g45g/mx66uw1g45g.c
)

# Library source files
set(LIBRARIES_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lcd/stm32_lcd.c)

# AI Postprocessing source files
set(AI_POSTPROCESSING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/ai-postprocessing-wrapper/app_postprocess_od_st_yolox_uf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/od_pp_st_yolox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp_maxi_if32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp_maxi_is8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp_maxi_iu8.c
)

# AI NPU Library source files
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_aton_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_sw_float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton/ll_sw_integer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6XX/mcu_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6XX/npu_cache.c
)

# Application source files
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_bqueue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_buffers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_cam.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_cpuload.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_nn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_postprocess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_ui.c)

# Network source files
set(NETWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Networks/Src/od_yolo_x_person.c)

# =============================================================================
# Include Directories Configuration
# =============================================================================
set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/Secure_nsclib
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/common/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/threadx/ports/cortex_m55/gnu/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/Components/Common
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/Device/STM32N6xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/DSP/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/imx335
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/evision/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lcd
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/Fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/ai-postprocessing-wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/ll_aton
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Npu/Devices/STM32N6xx
    ${CMAKE_CURRENT_SOURCE_DIR}/../Networks/Src)

# =============================================================================
# Compile Definitions
# =============================================================================
set(COMPILE_DEFINITIONS
    USE_HAL_DRIVER
    STM32N657xx
    TX_INCLUDE_USER_DEFINE_FILE
    TX_SINGLE_MODE_SECURE=1
    $<$<CONFIG:Debug>:DEBUG>
    LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6
    LL_ATON_OSAL=LL_ATON_OSAL_THREADX
    LL_ATON_RT_MODE=LL_ATON_RT_ASYNC
    APP_HAS_PARALLEL_NETWORKS=0
    LL_ATON_SW_FALLBACK
    LL_ATON_DBG_BUFFER_INFO_EXCLUDED=1)

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
# ThreadX Library
# =============================================================================
add_library(threadx OBJECT)
target_sources(threadx PRIVATE ${THREADX_SOURCES})
target_link_libraries(threadx PUBLIC stm32_interface)

# =============================================================================
# Main Executable
# =============================================================================
add_executable(${PROJECT_NAME})

# Application sources
target_sources(${PROJECT_NAME} PRIVATE ${APPLICATION_SOURCES})

# Additional source files
target_sources(
  ${PROJECT_NAME}
  PRIVATE ${BSP_SOURCES}
          ${APP_SOURCES}
          ${CMW_CAMERA_SOURCES}
          ${ISP_LIBRARY_SOURCES}
          ${LIBRARIES_SOURCES}
          ${AI_POSTPROCESSING_SOURCES}
          ${AI_NPU_SOURCES}
          ${NETWORK_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRECTORIES})

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFINITIONS})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE STM32_Drivers threadx
                                              ${TOOLCHAIN_LINK_LIBRARIES})

# Link directories
target_link_directories(
  ${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/evision/Lib
  ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Lib/GCC/ARMCortexM55)

# Link external libraries
target_link_libraries(
  ${PROJECT_NAME} PRIVATE :libn6-evision-st-ae_gcc.a :libn6-evision-awb_gcc.a
                          :NetworkRuntime1020_CM55_GCC.a m)

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
