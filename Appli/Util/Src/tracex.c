#include "tracex.h"

#ifdef TRACEX_ENABLE

#include "error.h"
#include "tx_api.h"
#include <stdbool.h>
#include <string.h>

#define TRACEX_REGISTRY_ENTRIES_COUNT 128U

/* Exported symbol for debugger memory dump (TraceX import). */
ULONG g_tracex_buffer[TRACEX_BUFFER_SIZE_BYTES / sizeof(ULONG)];
static bool tracex_started = false;

void TraceX_Init(void) {
  UINT status;

  if (tracex_started) {
    return;
  }

  status = tx_trace_enable(g_tracex_buffer, sizeof(g_tracex_buffer),
                           TRACEX_REGISTRY_ENTRIES_COUNT);
  APP_REQUIRE(status == TX_SUCCESS);
  tracex_started = true;
}

bool TraceX_IsEnabled(void) {
  return tracex_started;
}

uint32_t TraceX_GetBufferSize(void) {
  return (uint32_t)sizeof(g_tracex_buffer);
}

size_t TraceX_Read(uint32_t offset, uint8_t *dst, size_t max_len) {
  size_t avail;

  if (dst == NULL || max_len == 0U) {
    return 0U;
  }
  if (offset >= sizeof(g_tracex_buffer)) {
    return 0U;
  }

  avail = sizeof(g_tracex_buffer) - (size_t)offset;
  if (max_len > avail) {
    max_len = avail;
  }

  memcpy(dst, ((const uint8_t *)g_tracex_buffer) + offset, max_len);
  return max_len;
}

#else

void TraceX_Init(void) {
  /* TraceX is disabled at compile-time. */
}

bool TraceX_IsEnabled(void) {
  return false;
}

uint32_t TraceX_GetBufferSize(void) {
  return 0U;
}

size_t TraceX_Read(uint32_t offset, uint8_t *dst, size_t max_len) {
  (void)offset;
  (void)dst;
  (void)max_len;
  return 0U;
}

#endif /* TRACEX_ENABLE */
