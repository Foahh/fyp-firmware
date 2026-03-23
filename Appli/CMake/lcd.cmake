# =============================================================================
# LCD Library
# =============================================================================
set(LCD_SOURCES ${STM32CUBE_ROOT}/Utilities/lcd/stm32_lcd.c)

target_include_directories(
  stm32_interface INTERFACE ${STM32CUBE_ROOT}/Utilities/lcd
                            ${STM32CUBE_ROOT}/Utilities/Fonts)

add_library(LCD STATIC ${LCD_SOURCES})
target_link_libraries(LCD PUBLIC stm32_interface)
