set(NANOPB_ROOT "${EXTERNAL_ROOT}/nanopb")

add_library(Nanopb STATIC)

target_sources(Nanopb PRIVATE
  ${NANOPB_ROOT}/pb_encode.c
  ${NANOPB_ROOT}/pb_decode.c
  ${NANOPB_ROOT}/pb_common.c
  ${CMAKE_CURRENT_SOURCE_DIR}/Proto/nanopb/messages.pb.c)

target_include_directories(Nanopb PUBLIC
  ${NANOPB_ROOT}
  ${CMAKE_CURRENT_SOURCE_DIR}/Proto/nanopb)

target_link_libraries(Nanopb PRIVATE stm32_interface)
