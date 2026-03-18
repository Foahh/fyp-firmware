# =============================================================================
# STM32 HAL/LL Driver Library
# =============================================================================
set(STM32_DRIVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32n6xx_s.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ramcfg.c
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
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dcmipp.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma2d.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart_ex.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rif.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cacheaxi.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim.c
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim_ex.c
)

target_include_directories(
  stm32_interface
  INTERFACE
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Inc
    ${LIBRARY_ROOT}/STM32N6xx_HAL_Driver/Inc/Legacy
    ${LIBRARY_ROOT}/CMSIS/Device/ST/STM32N6xx/Include
    ${LIBRARY_ROOT}/CMSIS/Include
    ${LIBRARY_ROOT}/CMSIS/Device/STM32N6xx/Include
    ${LIBRARY_ROOT}/CMSIS/DSP/Include)

add_library(STM32_Drivers STATIC ${STM32_DRIVER_SOURCES})
target_link_libraries(STM32_Drivers PUBLIC stm32_interface)
