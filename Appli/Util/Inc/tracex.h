#ifndef TRACEX_H
#define TRACEX_H

#ifdef TRACEX_ENABLE
#include "tx_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TRACEX_BUFFER_SIZE_BYTES (64U * 1024U)
extern ULONG g_tracex_buffer[TRACEX_BUFFER_SIZE_BYTES / sizeof(ULONG)];
#endif

void TraceX_Init(void);
bool TraceX_IsEnabled(void);
uint32_t TraceX_GetBufferSize(void);
size_t TraceX_Read(uint32_t offset, uint8_t *dst, size_t max_len);

#endif /* TRACEX_H */
