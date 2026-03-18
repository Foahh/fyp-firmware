# =============================================================================
# LCD Library
# =============================================================================
set(LCD_SOURCES ${LIBRARY_ROOT}/lcd/stm32_lcd.c)

target_include_directories(
  stm32_interface INTERFACE ${LIBRARY_ROOT}/lcd
                            ${LIBRARY_ROOT}/Fonts)

add_library(LCD STATIC ${LCD_SOURCES})
target_link_libraries(LCD PUBLIC stm32_interface)
