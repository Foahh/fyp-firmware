#include "app_datalog.h"
#include "app_comm.h"
#include "app_error.h"
#include "messages.pb.h"
#include "pb_encode.h"
#include "stm32n6xx_hal.h"

#define FRAME_BUF_SIZE (4 + DatalogMessage_size)

/* Double buffer — encode into idle buffer while comm thread transmits the other */
static uint8_t frame_bufs[2][FRAME_BUF_SIZE];
static uint8_t buf_idx = 0;

void Datalog_Send_DetectionResult(const detection_info_t *info) {
  DatalogMessage msg = DatalogMessage_init_zero;
  msg.which_payload = DatalogMessage_detection_result_tag;

  DetectionResult *df = &msg.payload.detection_result;
  df->timestamp = HAL_GetTick();

  df->has_timing = true;
  df->timing.inference_ms = info->inference_ms;
  df->timing.postprocess_ms = info->postprocess_ms;
  df->timing.nn_period_ms = info->nn_period_ms;

  int n = info->nb_detect;
  if (n > DETECTION_MAX_BOXES) {
    n = DETECTION_MAX_BOXES;
  }
  df->detections_count = (pb_size_t)n;
  for (int i = 0; i < n; i++) {
    const od_pp_outBuffer_t *d = &info->detects[i];
    df->detections[i].x_center = d->x_center;
    df->detections[i].y_center = d->y_center;
    df->detections[i].width = d->width;
    df->detections[i].height = d->height;
    df->detections[i].conf = d->conf;
    df->detections[i].class_index = d->class_index;
  }

  uint8_t *buf = frame_bufs[buf_idx];

  pb_ostream_t stream = pb_ostream_from_buffer(buf + 4, FRAME_BUF_SIZE - 4);
  bool ok = pb_encode(&stream, DatalogMessage_fields, &msg);
  APP_REQUIRE(ok);

  uint32_t len = (uint32_t)stream.bytes_written;
  buf[0] = (uint8_t)(len);
  buf[1] = (uint8_t)(len >> 8);
  buf[2] = (uint8_t)(len >> 16);
  buf[3] = (uint8_t)(len >> 24);

  Comm_Send(buf, 4 + len);

  /* Flip to the other buffer for the next frame */
  buf_idx ^= 1;
}
