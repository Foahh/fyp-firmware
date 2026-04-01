# =============================================================================
# AI Postprocessing Library
# =============================================================================
set(AI_POSTPROCESSING_SOURCES
    ${LIBRARY_ROOT}/ai-postprocessing-wrapper/app_postprocess_od_st_yolox_ui.c
    ${LIBRARY_ROOT}/ai-postprocessing-wrapper/app_postprocess_od_yolo_d_ui.c
    ${LIBRARY_ROOT}/ai-postprocessing-wrapper/app_postprocess_od_yolov8_ui.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/od_pp_st_yolox.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/od_pp_yolo_d.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/od_pp_yolov8.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/vision_models_pp.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/vision_models_pp_maxi_if32.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/vision_models_pp_maxi_is8.c
    ${LIBRARY_ROOT}/lib_vision_models_pp/Src/vision_models_pp_maxi_iu8.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${LIBRARY_ROOT}/lib_vision_models_pp/Inc
            ${LIBRARY_ROOT}/ai-postprocessing-wrapper)

add_library(AI_Postprocessing STATIC ${AI_POSTPROCESSING_SOURCES})
target_link_libraries(AI_Postprocessing PUBLIC stm32_interface)
