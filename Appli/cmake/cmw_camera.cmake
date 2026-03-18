# =============================================================================
# Camera Middleware Library
# =============================================================================
set(CMW_CAMERA_SOURCES
    ${LIBRARY_ROOT}/stm32-mw-camera/cmw_camera.c
    ${LIBRARY_ROOT}/stm32-mw-camera/cmw_utils.c
    ${LIBRARY_ROOT}/stm32-mw-camera/sensors/cmw_imx335.c
    ${LIBRARY_ROOT}/stm32-mw-camera/sensors/imx335/imx335.c
    ${LIBRARY_ROOT}/stm32-mw-camera/sensors/imx335/imx335_reg.c
)

target_include_directories(
  stm32_interface
  INTERFACE
    ${LIBRARY_ROOT}/stm32-mw-camera
    ${LIBRARY_ROOT}/stm32-mw-camera/sensors
    ${LIBRARY_ROOT}/stm32-mw-camera/sensors/imx335)

add_library(CMW_Camera STATIC ${CMW_CAMERA_SOURCES})
target_link_libraries(CMW_Camera PUBLIC stm32_interface)
