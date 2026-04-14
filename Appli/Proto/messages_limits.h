#ifndef MESSAGES_LIMITS_H
#define MESSAGES_LIMITS_H

#include "messages.pb.h"
#include <stddef.h>

#define PB_MEMBER_ARRAY_COUNT(type, member) \
  (sizeof(((type *)0)->member) / sizeof(((type *)0)->member[0]))
#define PB_MEMBER_SIZE(type, member) sizeof(((type *)0)->member)

enum {
  PROTO_DETECTION_RESULT_MAX_DETECTIONS =
      (int)PB_MEMBER_ARRAY_COUNT(DetectionResult, detections),
  PROTO_DETECTION_RESULT_MAX_TRACKS =
      (int)PB_MEMBER_ARRAY_COUNT(DetectionResult, tracks),
  PROTO_TOF_RESULT_MAX_DEPTH_MM =
      (int)PB_MEMBER_ARRAY_COUNT(TofResult, depth_mm),
  PROTO_TOF_RESULT_MAX_RANGE_SIGMA_MM =
      (int)PB_MEMBER_ARRAY_COUNT(TofResult, range_sigma_mm),
  PROTO_TOF_RESULT_MAX_SIGNAL_PER_SPAD =
      (int)PB_MEMBER_ARRAY_COUNT(TofResult, signal_per_spad),
  PROTO_TOF_ALERT_MAX_PERSON_MM =
      (int)PB_MEMBER_ARRAY_COUNT(TofAlertResult, person_distances),
  PROTO_DEVICE_INFO_MODEL_NAME_CAPACITY =
      (int)PB_MEMBER_SIZE(DeviceInfo, model_name),
  PROTO_DEVICE_INFO_CLASS_LABEL_COUNT_CAPACITY =
      (int)PB_MEMBER_ARRAY_COUNT(DeviceInfo, class_labels),
  PROTO_DEVICE_INFO_CLASS_LABEL_CAPACITY =
      (int)sizeof(((DeviceInfo *)0)->class_labels[0]),
  PROTO_DEVICE_INFO_BUILD_TIMESTAMP_CAPACITY =
      (int)PB_MEMBER_SIZE(DeviceInfo, build_timestamp),
};

#undef PB_MEMBER_SIZE
#undef PB_MEMBER_ARRAY_COUNT

#endif /* MESSAGES_LIMITS_H */
