execute_process(
  COMMAND env TZ=Asia/Singapore date "+%Y-%m-%d %H:%M:%S"
  OUTPUT_VARIABLE ts
  OUTPUT_STRIP_TRAILING_WHITESPACE)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/build_timestamp.h"
  "#pragma once\n#define BUILD_TIMESTAMP \"${ts}\"\n")
