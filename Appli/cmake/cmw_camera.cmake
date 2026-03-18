# =============================================================================
# Camera Middleware Library
# =============================================================================
set(CMW_CAMERA_ROOT "${EXTERNAL_ROOT}/stm32-mw-camera")

set(CMW_CAMERA_SOURCES
    ${CMW_CAMERA_ROOT}/cmw_camera.c
    ${CMW_CAMERA_ROOT}/cmw_utils.c
    ${CMW_CAMERA_ROOT}/sensors/cmw_imx335.c
    ${CMW_CAMERA_ROOT}/sensors/imx335/imx335.c
    ${CMW_CAMERA_ROOT}/sensors/imx335/imx335_reg.c
)

target_include_directories(
  stm32_interface
  INTERFACE
    ${CMW_CAMERA_ROOT}
    ${CMW_CAMERA_ROOT}/sensors
    ${CMW_CAMERA_ROOT}/sensors/imx335)

add_library(CMW_Camera STATIC ${CMW_CAMERA_SOURCES})
target_link_libraries(CMW_Camera PUBLIC stm32_interface)
