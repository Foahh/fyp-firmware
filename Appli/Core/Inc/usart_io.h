/**
******************************************************************************
* @file    usart_io.h
* @author  Long Liangmao
*
******************************************************************************
* @attention
*
* Copyright (c) 2025 Long Liangmao.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
*/

#ifndef __USART_IO_H
#define __USART_IO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

int __io_putchar(int ch);
int __io_getchar(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_IO_H */

