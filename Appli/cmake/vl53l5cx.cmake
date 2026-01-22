# =============================================================================
# VL53L5CX ToF Sensor Library
# =============================================================================
set(VL53L5CX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/porting/platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/vl53l5cx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/modules/vl53l5cx_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/modules/vl53l5cx_plugin_xtalk.c
)

target_include_directories(stm32_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/vl53l5cx/porting
)

add_library(VL53L5CX STATIC ${VL53L5CX_SOURCES})
target_link_libraries(VL53L5CX PUBLIC stm32_interface)
