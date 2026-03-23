# =============================================================================
# ISP Library
# =============================================================================
set(ISP_ROOT "${EXTERNAL_ROOT}/stm32-mw-isp")

set(ISP_LIBRARY_SOURCES
    ${ISP_ROOT}/isp/Src/isp_ae_algo.c
    ${ISP_ROOT}/isp/Src/isp_algo.c
    ${ISP_ROOT}/isp/Src/isp_awb_algo.c
    ${ISP_ROOT}/isp/Src/isp_core.c
    ${ISP_ROOT}/isp/Src/isp_services.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${ISP_ROOT}/isp/Inc)

add_library(ISP_Library STATIC ${ISP_LIBRARY_SOURCES})
target_link_libraries(ISP_Library PUBLIC stm32_interface)
