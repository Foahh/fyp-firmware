# =============================================================================
# ISP Library
# =============================================================================
set(ISP_LIBRARY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-isp/isp/Src/isp_ae_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-isp/isp/Src/isp_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-isp/isp/Src/isp_awb_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-isp/isp/Src/isp_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-isp/isp/Src/isp_services.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/stm32-mw-isp/isp/Inc)

add_library(ISP_Library STATIC ${ISP_LIBRARY_SOURCES})
target_link_libraries(ISP_Library PUBLIC stm32_interface)
