# =============================================================================
# BSP Driver Library
# =============================================================================
set(BSP_SOURCES
    ${LIBRARY_ROOT}/BSP/STM32N6570-DK/stm32n6570_discovery.c
    ${LIBRARY_ROOT}/BSP/STM32N6570-DK/stm32n6570_discovery_bus.c
    ${LIBRARY_ROOT}/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
    ${LIBRARY_ROOT}/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
    ${LIBRARY_ROOT}/BSP/Components/aps256xx/aps256xx.c
    ${LIBRARY_ROOT}/BSP/Components/mx66uw1g45g/mx66uw1g45g.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${LIBRARY_ROOT}/BSP/STM32N6570-DK
            ${LIBRARY_ROOT}/BSP/Components/Common)

add_library(BSP STATIC ${BSP_SOURCES})
target_link_libraries(BSP PUBLIC stm32_interface STM32_Drivers)
