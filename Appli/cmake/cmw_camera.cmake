# =============================================================================
# Camera Middleware Library
# =============================================================================
set(CMW_CAMERA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/cmw_camera.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/cmw_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/cmw_imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/imx335/imx335.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/imx335/imx335_reg.c
)

target_include_directories(stm32_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-camera/sensors/imx335
)

add_library(CMW_Camera STATIC ${CMW_CAMERA_SOURCES})
target_link_libraries(CMW_Camera PUBLIC stm32_interface)
