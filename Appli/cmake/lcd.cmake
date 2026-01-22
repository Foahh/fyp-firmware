# =============================================================================
# LCD Library
# =============================================================================
set(LCD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lcd/stm32_lcd.c
)

target_include_directories(stm32_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lcd
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/Fonts
)

add_library(LCD STATIC ${LCD_SOURCES})
target_link_libraries(LCD PUBLIC stm32_interface)
