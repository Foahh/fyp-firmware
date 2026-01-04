/**
 ******************************************************************************
 * @file    app_event_bus.h
 * @author  Long Liangmao
 * @brief   Lightweight Event Bus (Publish/Subscribe) for ThreadX
 *          ISR-safe, deterministic, no dynamic allocation
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Long Liangmao.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef APP_EVENT_BUS_H
#define APP_EVENT_BUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "tx_api.h"
#include <stdint.h>
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/

/**
 * @brief  Event type identifier
 * @note   User should define their event types starting from 0
 */
typedef uint16_t event_bus_type_t;

/**
 * @brief  Event data payload (configurable size)
 * @note   Adjust EVENT_BUS_DATA_SIZE in app_event_bus.c to change payload size
 */
typedef struct {
  uint8_t data[16];  /* Default 16 bytes, can be adjusted via EVENT_BUS_DATA_SIZE */
} event_bus_data_t;

/**
 * @brief  Event structure
 */
typedef struct {
  event_bus_type_t type;  /* Event type */
  event_bus_data_t data;  /* Event payload */
  uint32_t timestamp;     /* Optional: timestamp when event was published */
} event_bus_event_t;

/**
 * @brief  Event bus handle
 */
typedef struct event_bus_s event_bus_t;

/**
 * @brief  Event callback function type
 * @param  event: Pointer to the received event
 * @param  user_data: User-provided context pointer
 */
typedef void (*event_bus_callback_t)(const event_bus_event_t *event, void *user_data);

/**
 * @brief  Subscriber handle (opaque)
 */
typedef struct event_bus_subscriber_s event_bus_subscriber_t;

/* Exported constants --------------------------------------------------------*/

/**
 * @brief  Maximum number of event types supported
 */
#define EVENT_BUS_MAX_TYPES 32

/* Application-specific event types */
typedef enum {
  EVENT_DEFAULT = 0,
  EVENT_MAX = EVENT_BUS_MAX_TYPES
} app_event_type_t;

/**
 * @brief  Maximum number of subscribers per event type
 */
#define EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE 8

/**
 * @brief  Queue depth for each subscriber (number of pending events)
 */
#define EVENT_BUS_SUBSCRIBER_QUEUE_DEPTH 4

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief  Initialize the event bus
 * @retval TX_SUCCESS on success, error code otherwise
 * @note   Must be called once before any other event bus operations
 */
UINT event_bus_init(void);

/**
 * @brief  Create a subscriber for one or more event types
 * @param  subscriber: Pointer to subscriber handle (output)
 * @param  name: Subscriber name (for debugging)
 * @param  callback: Callback function to invoke when events are received
 * @param  user_data: User context pointer passed to callback
 * @retval TX_SUCCESS on success, error code otherwise
 * @note   Subscriber must be destroyed with event_bus_subscriber_destroy()
 */
UINT event_bus_subscriber_create(event_bus_subscriber_t **subscriber,
                                 const char *name,
                                 event_bus_callback_t callback,
                                 void *user_data);

/**
 * @brief  Subscribe to an event type
 * @param  subscriber: Subscriber handle
 * @param  event_type: Event type to subscribe to
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT event_bus_subscribe(event_bus_subscriber_t *subscriber,
                         event_bus_type_t event_type);

/**
 * @brief  Unsubscribe from an event type
 * @param  subscriber: Subscriber handle
 * @param  event_type: Event type to unsubscribe from
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT event_bus_unsubscribe(event_bus_subscriber_t *subscriber,
                           event_bus_type_t event_type);

/**
 * @brief  Process pending events for a subscriber (call from subscriber's thread)
 * @param  subscriber: Subscriber handle
 * @param  timeout: Timeout in ThreadX ticks (TX_NO_WAIT, TX_WAIT_FOREVER, or ticks)
 * @retval TX_SUCCESS if event processed, TX_QUEUE_EMPTY if no events, error otherwise
 * @note   This function should be called periodically from the subscriber's thread
 */
UINT event_bus_subscriber_process(event_bus_subscriber_t *subscriber,
                                  ULONG timeout);

/**
 * @brief  Destroy a subscriber and free its resources
 * @param  subscriber: Subscriber handle (set to NULL after call)
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT event_bus_subscriber_destroy(event_bus_subscriber_t **subscriber);

/**
 * @brief  Publish an event (thread-safe, ISR-safe)
 * @param  event_type: Event type
 * @param  data: Pointer to event data (can be NULL)
 * @retval TX_SUCCESS on success, error code otherwise
 * @note   Can be called from thread context or ISR context
 * @note   O(1) operation - bounded time
 */
UINT event_bus_publish(event_bus_type_t event_type, const event_bus_data_t *data);

/**
 * @brief  Publish an event with timestamp (thread-safe, ISR-safe)
 * @param  event_type: Event type
 * @param  data: Pointer to event data (can be NULL)
 * @param  timestamp: Timestamp value
 * @retval TX_SUCCESS on success, error code otherwise
 * @note   Can be called from thread context or ISR context
 */
UINT event_bus_publish_with_timestamp(event_bus_type_t event_type,
                                      const event_bus_data_t *data,
                                      uint32_t timestamp);

/**
 * @brief  Get number of pending events for a subscriber
 * @param  subscriber: Subscriber handle
 * @param  count: Pointer to store the count (output)
 * @retval TX_SUCCESS on success, error code otherwise
 */
UINT event_bus_subscriber_get_pending_count(event_bus_subscriber_t *subscriber,
                                             ULONG *count);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_BUS_H */

