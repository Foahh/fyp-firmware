# =============================================================================
# VL53L5CX ToF Sensor Library
# =============================================================================
set(VL53L5CX_ROOT "${EXTERNAL_ROOT}/stm32-vl53l5cx")

set(VL53L5CX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/vl53l5cx_platform.c
    ${VL53L5CX_ROOT}/vl53l5cx.c
    ${VL53L5CX_ROOT}/modules/vl53l5cx_api.c
    ${VL53L5CX_ROOT}/modules/vl53l5cx_plugin_detection_thresholds.c
    ${VL53L5CX_ROOT}/modules/vl53l5cx_plugin_motion_indicator.c
    ${VL53L5CX_ROOT}/modules/vl53l5cx_plugin_xtalk.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${VL53L5CX_ROOT}
            ${VL53L5CX_ROOT}/modules)

add_library(VL53L5CX STATIC ${VL53L5CX_SOURCES})
target_link_libraries(VL53L5CX PUBLIC stm32_interface)
