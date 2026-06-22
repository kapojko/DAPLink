/**
 * @file    Debug.h
 * @brief   Lightweight debug print and delay helpers for STM32H723VG port.
 *
 * Mirrors the API of the demo's User/Debug.h so the same PRINT(...) calls
 * work in both the demo and the port. PRINT is a no-op unless DEBUG is
 * defined (set as a compiler flag when needed).
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "stm32h7xx_hal_def.h"
#include <stdarg.h>

#if defined(DEBUG_PRINT) && (defined(TRANSMIT_DEBUG) || !defined(TRANSMIT_UART))
#define PRINT(format, ...) DebugPrint(format, ##__VA_ARGS__)
#else
#define PRINT(...)        ((void)0)
#endif

HAL_StatusTypeDef InitDebug(void);

void DebugPrint(const char *fmt, ...);

void DelayMsFunc(int ms);
void DelayUsFunc(int us);

#endif /* __DEBUG_H__ */