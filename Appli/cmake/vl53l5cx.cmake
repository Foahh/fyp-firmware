# =============================================================================
# VL53L5CX ToF Sensor Library
# =============================================================================
set(VL53L5CX_SOURCES
    ${LIBRARY_ROOT}/vl53l5cx/porting/platform.c
    ${LIBRARY_ROOT}/vl53l5cx/vl53l5cx.c
    ${LIBRARY_ROOT}/vl53l5cx/modules/vl53l5cx_api.c
    ${LIBRARY_ROOT}/vl53l5cx/modules/vl53l5cx_plugin_detection_thresholds.c
    ${LIBRARY_ROOT}/vl53l5cx/modules/vl53l5cx_plugin_motion_indicator.c
    ${LIBRARY_ROOT}/vl53l5cx/modules/vl53l5cx_plugin_xtalk.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${LIBRARY_ROOT}/vl53l5cx
            ${LIBRARY_ROOT}/vl53l5cx/modules
            ${LIBRARY_ROOT}/vl53l5cx/porting)

add_library(VL53L5CX STATIC ${VL53L5CX_SOURCES})
target_link_libraries(VL53L5CX PUBLIC stm32_interface)
