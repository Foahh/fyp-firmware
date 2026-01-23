# =============================================================================
# AI Postprocessing Library
# =============================================================================
set(AI_POSTPROCESSING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/ai-postprocessing-wrapper/app_postprocess_od_st_yolox_ui.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/od_pp_st_yolox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp_maxi_if32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp_maxi_is8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Src/vision_models_pp_maxi_iu8.c
)

target_include_directories(
  stm32_interface
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/lib_vision_models_pp/Inc
            ${CMAKE_CURRENT_SOURCE_DIR}/../Libraries/ai-postprocessing-wrapper)

add_library(AI_Postprocessing STATIC ${AI_POSTPROCESSING_SOURCES})
target_link_libraries(AI_Postprocessing PUBLIC stm32_interface)
