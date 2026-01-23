# =============================================================================
# STM32 HAL/LL Driver Library
# =============================================================================
set(STM32_DRIVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32n6xx_s.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Src/stm32n6xx_hal_ramcfg.c
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

target_include_directories(
  stm32_interface
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32N6xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/Device/ST/STM32N6xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/Device/STM32N6xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/CMSIS/DSP/Include)

add_library(STM32_Drivers STATIC ${STM32_DRIVER_SOURCES})
target_link_libraries(STM32_Drivers PUBLIC stm32_interface)
