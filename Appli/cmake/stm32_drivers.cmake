# =============================================================================
# STM32 HAL/LL Driver Library
# =============================================================================
set(STM32_DRIVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32n6xx_s.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ramcfg.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cortex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rcc_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_gpio.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_pwr_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_exti.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_xspi.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dcmipp.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_i2c_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ltdc_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_dma2d.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_uart_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_usart_ex.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_rif.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_cacheaxi.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim.c
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_tim_ex.c
)

target_include_directories(
  stm32_interface
  INTERFACE
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Inc
    ${STM32CUBE_ROOT}/Drivers/STM32N6xx_HAL_Driver/Inc/Legacy
    ${STM32CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32N6xx/Include
    ${STM32CUBE_ROOT}/Drivers/CMSIS/Include
    ${STM32CUBE_ROOT}/Drivers/CMSIS/Device/STM32N6xx/Include
    ${STM32CUBE_ROOT}/Drivers/CMSIS/DSP/Include)

add_library(STM32_Drivers STATIC ${STM32_DRIVER_SOURCES})
target_link_libraries(STM32_Drivers PUBLIC stm32_interface)
