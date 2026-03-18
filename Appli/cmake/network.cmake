# =============================================================================
# Network Library
# =============================================================================
if(NOT DEFINED NETWORK_NAME)
  message(
    FATAL_ERROR
      "NETWORK_NAME is not defined. Pass -DNETWORK_NAME=<name> from build.py.")
endif()

set(NETWORK_SOURCES
    ${FIRMWARE_ROOT}/Networks/Src/${NETWORK_NAME}.c
    ${FIRMWARE_ROOT}/Networks/Src/stai_${NETWORK_NAME}.c)

target_include_directories(
  stm32_interface INTERFACE ${FIRMWARE_ROOT}/Networks/Src)

add_library(Network STATIC ${NETWORK_SOURCES})
target_link_libraries(Network PUBLIC stm32_interface AI_NPU)

target_link_directories(
  Network PUBLIC
  ${LIBRARY_ROOT}/AI/Lib/GCC/ARMCortexM55)

target_link_libraries(Network PUBLIC :NetworkRuntime1200_CM55_GCC.a)
