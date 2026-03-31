# =============================================================================
# Tracker Library
# =============================================================================
set(TRACKER_SOURCES
    ${LIBRARY_ROOT}/tracker/kf.c
    ${LIBRARY_ROOT}/tracker/tracker.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${LIBRARY_ROOT}/tracker)

add_library(Tracker STATIC ${TRACKER_SOURCES})
target_link_libraries(Tracker PUBLIC stm32_interface)
