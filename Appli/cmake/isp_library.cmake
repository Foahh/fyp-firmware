# =============================================================================
# ISP Library
# =============================================================================
set(ISP_LIBRARY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Src/isp_algo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Src/isp_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Src/isp_services.c
)

target_include_directories(stm32_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/isp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/evision/Inc
)

add_library(ISP_Library STATIC ${ISP_LIBRARY_SOURCES})
target_link_libraries(ISP_Library PUBLIC stm32_interface)

target_link_directories(ISP_Library PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/STM32_ISP_Library/evision/Lib
)

target_link_libraries(ISP_Library PUBLIC
    :libn6-evision-st-ae_gcc.a
    :libn6-evision-awb_gcc.a
)
