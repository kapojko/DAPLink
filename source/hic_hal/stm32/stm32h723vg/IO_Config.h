/**
 * @file    IO_Config.h
 * @brief   Board-level pin and DUT configuration for STM32H723VG.
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __IO_CONFIG_H__
#define __IO_CONFIG_H__

#include "stm32h7xx.h"
#include "compiler.h"
#include "daplink.h"

// The H7 HAL defines UART_PARITY_NONE/EVEN/ODD as numeric macros,
// which collide with the DAPLink uart.h enum of the same names.
// Undefine the HAL versions here so uart.h can declare its enum
// cleanly from any TU that includes DAP_config.h.
#ifdef UART_PARITY_NONE
#undef UART_PARITY_NONE
#endif
#ifdef UART_PARITY_EVEN
#undef UART_PARITY_EVEN
#endif
#ifdef UART_PARITY_ODD
#undef UART_PARITY_ODD
#endif

COMPILER_ASSERT(DAPLINK_HIC_ID == DAPLINK_HIC_ID_STM32H723VG);

// All LEDs are active-low. Set HIGH to keep them off.

// Connected LED (PA6)
#define CONNECTED_LED_PORT                GPIOA
#define CONNECTED_LED_PIN                 GPIO_PIN_6
#define CONNECTED_LED_PIN_Bit             6

// Running LED (PA7)
#define RUNNING_LED_PORT                  GPIOA
#define RUNNING_LED_PIN                   GPIO_PIN_7
#define RUNNING_LED_PIN_Bit               7

// HID LED (PD14)
#define PIN_HID_LED_PORT                  GPIOD
#define PIN_HID_LED                       GPIO_PIN_14
#define PIN_HID_LED_Bit                   14

// CDC LED (PD12)
#define PIN_CDC_LED_PORT                  GPIOD
#define PIN_CDC_LED                       GPIO_PIN_12
#define PIN_CDC_LED_Bit                   12

// MSC LED (PD13)
#define PIN_MSC_LED_PORT                  GPIOD
#define PIN_MSC_LED                       GPIO_PIN_13
#define PIN_MSC_LED_Bit                   13

// Extra LED (PB0)
#define EXTRA_LED_PORT                    GPIOB
#define EXTRA_LED_PIN                     GPIO_PIN_0
#define EXTRA_LED_PIN_Bit                 0

// Power - drives the target power switch (PE0). Default off.
#define POWER_EN_PIN_PORT                 GPIOE
#define POWER_EN_PIN                      GPIO_PIN_0
#define POWER_EN_PIN_Bit                  0

// SWD - direct bit-bang on these pins (no buffer, no level shifter).
// SWDIO has separate output and input pins (PE2 out, PE3 in).
#define SWCLK_TCK_PIN_PORT                GPIOE
#define SWCLK_TCK_PIN                     GPIO_PIN_5
#define SWCLK_TCK_PIN_Bit                 5

#define SWDIO_OUT_PIN_PORT                GPIOE
#define SWDIO_OUT_PIN                     GPIO_PIN_2
#define SWDIO_OUT_PIN_Bit                 2

#define SWDIO_IN_PIN_PORT                 GPIOE
#define SWDIO_IN_PIN                      GPIO_PIN_3
#define SWDIO_IN_PIN_Bit                  3

// Buttons - both wired to EXTI9_5_IRQn. PB8 (BTN_BOOT) is used as the
// alternate reset-button source.
#define BTN_BOOT_PORT                     GPIOB
#define BTN_BOOT_PIN                      GPIO_PIN_8
#define BTN_BOOT_PIN_Bit                  8
#define BTN_BOOT_EXTI_IRQn                EXTI9_5_IRQn

#define BTN_CONF_PORT                     GPIOB
#define BTN_CONF_PIN                      GPIO_PIN_9
#define BTN_CONF_PIN_Bit                  9
#define BTN_CONF_EXTI_IRQn                EXTI9_5_IRQn

#endif // __IO_CONFIG_H__
