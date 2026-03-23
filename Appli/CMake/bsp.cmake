# =============================================================================
# BSP Driver Library
# =============================================================================
set(BSP_SOURCES
    ${STM32CUBE_ROOT}/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery.c
    ${STM32CUBE_ROOT}/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
    ${STM32CUBE_ROOT}/Drivers/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/aps256xx/aps256xx.c
    ${STM32CUBE_ROOT}/Drivers/BSP/Components/mx66uw1g45g/mx66uw1g45g.c

    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/stm32n6570_discovery_bus.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${STM32CUBE_ROOT}/Drivers/BSP/STM32N6570-DK
            ${STM32CUBE_ROOT}/Drivers/BSP/Components/Common)

add_library(BSP STATIC ${BSP_SOURCES})
target_link_libraries(BSP PUBLIC stm32_interface STM32_Drivers threadx)
