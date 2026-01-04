/**
 ******************************************************************************
 * @file    app_event_bus.c
 * @author  Long Liangmao
 * @brief   Lightweight Event Bus (Publish/Subscribe) implementation for ThreadX
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

/* Includes ------------------------------------------------------------------*/
#include "app_event_bus.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/

/* Adjust this to change event data payload size (must match header) */
#define EVENT_BUS_DATA_SIZE 16

/* Static storage for event bus */
#define EVENT_BUS_STORAGE_SIZE \
  (EVENT_BUS_MAX_TYPES * EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE)

/* Private typedef -----------------------------------------------------------*/

/**
 * @brief  Internal subscriber structure
 */
struct event_bus_subscriber_s {
  TX_QUEUE queue;                    /* Queue for receiving events */
  event_bus_callback_t callback;      /* Callback function */
  void *user_data;                    /* User context */
  char name[16];                      /* Subscriber name (for debugging) */
  bool active;                        /* Whether subscriber is active */
  UCHAR queue_storage[EVENT_BUS_SUBSCRIBER_QUEUE_DEPTH * sizeof(event_bus_event_t)];
};

/**
 * @brief  Event type subscription list
 */
typedef struct {
  event_bus_subscriber_t *subscribers[EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE];
  uint8_t count;                     /* Number of active subscribers */
} event_type_subscribers_t;

/**
 * @brief  Event bus structure
 */
struct event_bus_s {
  TX_MUTEX mutex;                    /* Mutex for subscriber list protection */
  event_type_subscribers_t types[EVENT_BUS_MAX_TYPES];
  bool initialized;
};

/* Private variables ---------------------------------------------------------*/

/* Static storage for event bus */
static event_bus_t g_event_bus;

/* Static pool of subscriber structures */
static event_bus_subscriber_t g_subscriber_pool[EVENT_BUS_STORAGE_SIZE];
static bool g_subscriber_pool_used[EVENT_BUS_STORAGE_SIZE];

/* Private function prototypes -----------------------------------------------*/

static event_bus_subscriber_t *alloc_subscriber(void);
static void free_subscriber(event_bus_subscriber_t *subscriber);
static UINT add_subscriber_to_type(event_bus_type_t event_type,
                                   event_bus_subscriber_t *subscriber);
static UINT remove_subscriber_from_type(event_bus_type_t event_type,
                                        event_bus_subscriber_t *subscriber);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Allocate a subscriber from the static pool
 * @retval Pointer to subscriber or NULL if pool exhausted
 */
static event_bus_subscriber_t *alloc_subscriber(void) {
  for (uint32_t i = 0; i < EVENT_BUS_STORAGE_SIZE; i++) {
    if (!g_subscriber_pool_used[i]) {
      g_subscriber_pool_used[i] = true;
      memset(&g_subscriber_pool[i], 0, sizeof(event_bus_subscriber_t));
      return &g_subscriber_pool[i];
    }
  }
  return NULL;
}

/**
 * @brief  Free a subscriber back to the pool
 * @param  subscriber: Subscriber to free
 */
static void free_subscriber(event_bus_subscriber_t *subscriber) {
  if (subscriber == NULL) {
    return;
  }

  /* Find subscriber in pool */
  for (uint32_t i = 0; i < EVENT_BUS_STORAGE_SIZE; i++) {
    if (&g_subscriber_pool[i] == subscriber) {
      g_subscriber_pool_used[i] = false;
      subscriber->active = false;
      return;
    }
  }
}

/**
 * @brief  Add subscriber to an event type's subscription list
 * @param  event_type: Event type
 * @param  subscriber: Subscriber to add
 * @retval TX_SUCCESS on success, error code otherwise
 */
static UINT add_subscriber_to_type(event_bus_type_t event_type,
                                   event_bus_subscriber_t *subscriber) {
  if (event_type >= EVENT_BUS_MAX_TYPES) {
    return TX_SIZE_ERROR;
  }

  event_type_subscribers_t *type_subs = &g_event_bus.types[event_type];

  /* Check if already subscribed */
  for (uint8_t i = 0; i < type_subs->count; i++) {
    if (type_subs->subscribers[i] == subscriber) {
      return TX_SUCCESS; /* Already subscribed */
    }
  }

  /* Check if room available */
  if (type_subs->count >= EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE) {
    return TX_NO_MEMORY;
  }

  /* Add subscriber */
  type_subs->subscribers[type_subs->count++] = subscriber;
  return TX_SUCCESS;
}

/**
 * @brief  Remove subscriber from an event type's subscription list
 * @param  event_type: Event type
 * @param  subscriber: Subscriber to remove
 * @retval TX_SUCCESS on success, error code otherwise
 */
static UINT remove_subscriber_from_type(event_bus_type_t event_type,
                                        event_bus_subscriber_t *subscriber) {
  if (event_type >= EVENT_BUS_MAX_TYPES) {
    return TX_SIZE_ERROR;
  }

  event_type_subscribers_t *type_subs = &g_event_bus.types[event_type];

  /* Find and remove subscriber */
  for (uint8_t i = 0; i < type_subs->count; i++) {
    if (type_subs->subscribers[i] == subscriber) {
      /* Shift remaining subscribers */
      for (uint8_t j = i; j < type_subs->count - 1; j++) {
        type_subs->subscribers[j] = type_subs->subscribers[j + 1];
      }
      type_subs->count--;
      return TX_SUCCESS;
    }
  }

  return TX_SUCCESS; /* Not found, but that's okay */
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initialize the event bus
 */
UINT event_bus_init(void) {
  UINT status;

  if (g_event_bus.initialized) {
    return TX_SUCCESS; /* Already initialized */
  }

  /* Initialize mutex */
  status = tx_mutex_create(&g_event_bus.mutex, "event_bus_mutex", TX_NO_INHERIT);
  if (status != TX_SUCCESS) {
    return status;
  }

  /* Initialize subscriber pool */
  memset(g_subscriber_pool_used, 0, sizeof(g_subscriber_pool_used));
  memset(g_event_bus.types, 0, sizeof(g_event_bus.types));

  g_event_bus.initialized = true;
  return TX_SUCCESS;
}

/**
 * @brief  Create a subscriber
 */
UINT event_bus_subscriber_create(event_bus_subscriber_t **subscriber,
                                 const char *name,
                                 event_bus_callback_t callback,
                                 void *user_data) {
  UINT status;
  event_bus_subscriber_t *sub;

  if (!g_event_bus.initialized) {
    return TX_NOT_DONE;
  }

  if (subscriber == NULL || callback == NULL) {
    return TX_PTR_ERROR;
  }

  /* Allocate subscriber from pool */
  sub = alloc_subscriber();
  if (sub == NULL) {
    return TX_NO_MEMORY;
  }

  /* Create queue for subscriber */
  /* Calculate message size in ULONG units (round up) */
  ULONG msg_size_ulongs = (sizeof(event_bus_event_t) + sizeof(ULONG) - 1) / sizeof(ULONG);
  status = tx_queue_create(&sub->queue, "sub_queue",
                           msg_size_ulongs,
                           sub->queue_storage,
                           sizeof(sub->queue_storage));
  if (status != TX_SUCCESS) {
    free_subscriber(sub);
    return status;
  }

  /* Initialize subscriber */
  sub->callback = callback;
  sub->user_data = user_data;
  sub->active = true;
  if (name != NULL) {
    strncpy(sub->name, name, sizeof(sub->name) - 1);
    sub->name[sizeof(sub->name) - 1] = '\0';
  } else {
    sub->name[0] = '\0';
  }

  *subscriber = sub;
  return TX_SUCCESS;
}

/**
 * @brief  Subscribe to an event type
 */
UINT event_bus_subscribe(event_bus_subscriber_t *subscriber,
                         event_bus_type_t event_type) {
  UINT status;

  if (!g_event_bus.initialized) {
    return TX_NOT_DONE;
  }

  if (subscriber == NULL || !subscriber->active) {
    return TX_PTR_ERROR;
  }

  if (event_type >= EVENT_BUS_MAX_TYPES) {
    return TX_SIZE_ERROR;
  }

  /* Acquire mutex (thread context only) */
  status = tx_mutex_get(&g_event_bus.mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return status;
  }

  status = add_subscriber_to_type(event_type, subscriber);

  tx_mutex_put(&g_event_bus.mutex);
  return status;
}

/**
 * @brief  Unsubscribe from an event type
 */
UINT event_bus_unsubscribe(event_bus_subscriber_t *subscriber,
                           event_bus_type_t event_type) {
  UINT status;

  if (!g_event_bus.initialized) {
    return TX_NOT_DONE;
  }

  if (subscriber == NULL) {
    return TX_PTR_ERROR;
  }

  if (event_type >= EVENT_BUS_MAX_TYPES) {
    return TX_SIZE_ERROR;
  }

  /* Acquire mutex (thread context only) */
  status = tx_mutex_get(&g_event_bus.mutex, TX_WAIT_FOREVER);
  if (status != TX_SUCCESS) {
    return status;
  }

  status = remove_subscriber_from_type(event_type, subscriber);

  tx_mutex_put(&g_event_bus.mutex);
  return status;
}

/**
 * @brief  Process pending events for a subscriber
 */
UINT event_bus_subscriber_process(event_bus_subscriber_t *subscriber,
                                  ULONG timeout) {
  UINT status;
  event_bus_event_t event;

  if (subscriber == NULL || !subscriber->active) {
    return TX_PTR_ERROR;
  }

  /* Receive event from queue */
  status = tx_queue_receive(&subscriber->queue, &event, timeout);
  if (status != TX_SUCCESS) {
    return status;
  }

  /* Invoke callback */
  if (subscriber->callback != NULL) {
    subscriber->callback(&event, subscriber->user_data);
  }

  return TX_SUCCESS;
}

/**
 * @brief  Destroy a subscriber
 */
UINT event_bus_subscriber_destroy(event_bus_subscriber_t **subscriber) {
  UINT status;

  if (subscriber == NULL || *subscriber == NULL) {
    return TX_PTR_ERROR;
  }

  event_bus_subscriber_t *sub = *subscriber;

  /* Unsubscribe from all event types */
  if (g_event_bus.initialized) {
    status = tx_mutex_get(&g_event_bus.mutex, TX_WAIT_FOREVER);
    if (status == TX_SUCCESS) {
      for (event_bus_type_t type = 0; type < EVENT_BUS_MAX_TYPES; type++) {
        remove_subscriber_from_type(type, sub);
      }
      tx_mutex_put(&g_event_bus.mutex);
    }
  }

  /* Delete queue */
  tx_queue_delete(&sub->queue);

  /* Free subscriber */
  free_subscriber(sub);
  *subscriber = NULL;

  return TX_SUCCESS;
}

/**
 * @brief  Publish an event (ISR-safe, thread-safe)
 */
UINT event_bus_publish(event_bus_type_t event_type,
                       const event_bus_data_t *data) {
  return event_bus_publish_with_timestamp(event_type, data, 0);
}

/**
 * @brief  Publish an event with timestamp (ISR-safe, thread-safe)
 */
UINT event_bus_publish_with_timestamp(event_bus_type_t event_type,
                                      const event_bus_data_t *data,
                                      uint32_t timestamp) {
  event_bus_event_t event;
  event_type_subscribers_t *type_subs;
  UINT status;

  if (!g_event_bus.initialized) {
    return TX_NOT_DONE;
  }

  if (event_type >= EVENT_BUS_MAX_TYPES) {
    return TX_SIZE_ERROR;
  }

  /* Prepare event */
  event.type = event_type;
  event.timestamp = timestamp;
  if (data != NULL) {
    memcpy(&event.data, data, sizeof(event_bus_data_t));
  } else {
    memset(&event.data, 0, sizeof(event_bus_data_t));
  }

  /* Get subscriber list for this event type */
  type_subs = &g_event_bus.types[event_type];

  /* Publish to all subscribers (O(1) per subscriber, bounded) */
  /* Note: We don't acquire mutex here because:
   * 1. Subscriber list is only modified during subscribe/unsubscribe (thread context)
   * 2. We only read the list during publish
   * 3. Reading the count and array is atomic for our use case
   * 4. Worst case: we might miss a subscriber that was just added, which is acceptable
   */
  uint8_t count = type_subs->count; /* Read count once for consistency */

  for (uint8_t i = 0; i < count; i++) {
    event_bus_subscriber_t *sub = type_subs->subscribers[i];
    if (sub != NULL && sub->active) {
      /* Send to queue (ISR-safe with TX_NO_WAIT) */
      /* Both tx_queue_send and tx_queue_front_send are ISR-safe with TX_NO_WAIT */
      status = tx_queue_send(&sub->queue, &event, TX_NO_WAIT);

      /* Ignore queue full errors - subscriber will miss this event */
      (void)status;
    }
  }

  return TX_SUCCESS;
}

/**
 * @brief  Get number of pending events for a subscriber
 */
UINT event_bus_subscriber_get_pending_count(event_bus_subscriber_t *subscriber,
                                             ULONG *count) {
  if (subscriber == NULL || count == NULL) {
    return TX_PTR_ERROR;
  }

  return tx_queue_info_get(&subscriber->queue, TX_NULL, count,
                           TX_NULL, TX_NULL, TX_NULL, TX_NULL);
}

