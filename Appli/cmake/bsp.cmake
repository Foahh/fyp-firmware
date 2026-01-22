# =============================================================================
# BSP Driver Library
# =============================================================================
set(BSP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery_bus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery_xspi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK/stm32n6570_discovery_lcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/Components/aps256xx/aps256xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/Components/mx66uw1g45g/mx66uw1g45g.c
)

target_include_directories(stm32_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/STM32N6570-DK
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/BSP/Components/Common
)

add_library(BSP STATIC ${BSP_SOURCES})
target_link_libraries(BSP PUBLIC stm32_interface STM32_Drivers)

