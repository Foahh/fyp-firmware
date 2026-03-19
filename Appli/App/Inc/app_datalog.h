#ifndef APP_DATALOG_H
#define APP_DATALOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_postprocess.h"

/**
 * @brief  Send object detection results for the current frame
 * @param  info: Pointer to detection info (from Postprocess_GetInfo)
 */
void Datalog_Send_DetectionResult(const detection_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATALOG_H */
