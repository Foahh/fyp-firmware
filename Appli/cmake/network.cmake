# =============================================================================
# Network Library
# =============================================================================
if(NOT DEFINED NETWORK_NAME)
  message(
    FATAL_ERROR
      "NETWORK_NAME is not defined. Pass -DNETWORK_NAME=<name> from build.py.")
endif()

set(NETWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Networks/Src/${NETWORK_NAME}.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Networks/Src/stai_${NETWORK_NAME}.c)

target_include_directories(
  stm32_interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../Networks/Src)

add_library(Network STATIC ${NETWORK_SOURCES})
target_link_libraries(Network PUBLIC stm32_interface AI_NPU)

target_link_directories(
  Network PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/AI/Lib/GCC/ARMCortexM55)

target_link_libraries(Network PUBLIC :NetworkRuntime1200_CM55_GCC.a)
