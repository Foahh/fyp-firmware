/**
******************************************************************************
* @file    utils.h
* @author  Long Liangmao
*
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

#ifndef UTILS
#define UTILS

/* Align to 32 bytes */
#define ALIGN_32 __attribute__((aligned(32)))
/* In PSRAM section */
#define IN_PSRAM __attribute__((section(".psram_bss")))
/* Align to a value */
#define ALIGN_VALUE(v, a) (((v) + (a) - 1) & ~((a) - 1))

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Compiler memory barrier - prevents reordering of memory operations */
#define MEMORY_BARRIER() __asm__ volatile("" ::: "memory")

#define IS_IRQ_MODE() (__get_IPSR() != 0U)

#endif