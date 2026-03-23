# =============================================================================
# ISM330DLC IMU Sensor Library
# =============================================================================
set(ISM330DLC_ROOT "${EXTERNAL_ROOT}/stm32-ism330dlc")

set(ISM330DLC_SOURCES
    ${ISM330DLC_ROOT}/ism330dlc.c
    ${ISM330DLC_ROOT}/ism330dlc_reg.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${ISM330DLC_ROOT})

add_library(ISM330DLC STATIC ${ISM330DLC_SOURCES})
target_link_libraries(ISM330DLC PUBLIC stm32_interface)
