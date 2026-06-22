/**
 * @file    DAP_config.h
 * @brief   CMSIS-DAP Hardware I/O pin access for STM32H723VG.
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2021, ARM Limited, All Rights Reserved
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

#ifndef __DAP_CONFIG_H__
#define __DAP_CONFIG_H__

#include "IO_Config.h"

//**************************************************************************************************
/**
\defgroup DAP_Config_Debug_gr CMSIS-DAP Debug Unit Information
\ingroup DAP_ConfigIO_gr
@{
Provides definitions about the hardware and configuration of the Debug Unit.
*/

/// Processor Clock of the Cortex-M MCU used in the Debug Unit.
#define CPU_CLOCK               SystemCoreClock

/// Number of processor cycles for I/O Port write operations.
#define IO_PORT_WRITE_CYCLES    2U

/// Indicate that Serial Wire Debug (SWD) communication mode is available.
#define DAP_SWD                 1

/// Indicate that JTAG communication mode is available.
#define DAP_JTAG                0

/// Configure maximum number of JTAG devices on the scan chain.
#define DAP_JTAG_DEV_CNT        0

/// Default communication mode on the Debug Access Port.
#define DAP_DEFAULT_PORT        1

/// Default communication speed on the Debug Access Port for SWD and JTAG mode.
#define DAP_DEFAULT_SWJ_CLOCK   5000000

/// Maximum Package Size for Command and Response data.
#ifndef HID_ENDPOINT
#define DAP_PACKET_SIZE         512
#else
#define DAP_PACKET_SIZE         64
#endif

/// Maximum Package Buffers for Command and Response data.
#define DAP_PACKET_COUNT        4

/// Indicate that UART Serial Wire Output (SWO) trace is available.
#define SWO_UART                0

/// USART Driver instance number for the UART SWO.
#define SWO_UART_DRIVER         0

/// Maximum SWO UART Baudrate
#define SWO_UART_MAX_BAUDRATE   10000000U

/// Indicate that Manchester Serial Wire Output (SWO) trace is available.
#define SWO_MANCHESTER          0

/// SWO Trace Buffer Size.
#define SWO_BUFFER_SIZE         4096U

/// SWO Streaming Trace.
#define SWO_STREAM              0

/// Clock frequency of the Test Domain Timer.
#define TIMESTAMP_CLOCK         1000000U

/// Indicate that UART Communication Port is available.
#define DAP_UART                0

/// USART Driver instance number for the UART Communication Port.
#define DAP_UART_DRIVER         1

/// UART Receive Buffer Size.
#define DAP_UART_RX_BUFFER_SIZE 1024U

/// UART Transmit Buffer Size.
#define DAP_UART_TX_BUFFER_SIZE 1024U

/// Indicate that UART Communication via USB COM Port is available.
#define DAP_UART_USB_COM_PORT   1

/// Debug Unit is connected to fixed Target Device.
#define TARGET_FIXED            0

///@}

//**************************************************************************************************
/**
\defgroup DAP_Config_PortIO_gr CMSIS-DAP Hardware I/O Pin Access
*/

// Configure DAP I/O pins ------------------------------

__STATIC_INLINE void PORT_JTAG_SETUP(void) {}

/** Setup SWD I/O pins: SWCLK, SWDIO. */
__STATIC_INLINE void PORT_SWD_SETUP(void)
{
    GPIO_InitTypeDef gpio_init =
    {
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Speed = GPIO_SPEED_FREQ_HIGH,
    };

    // SWCLK output, idle HIGH
    gpio_init.Pin = SWCLK_TCK_PIN;
    HAL_GPIO_Init(SWCLK_TCK_PIN_PORT, &gpio_init);
    HAL_GPIO_WritePin(SWCLK_TCK_PIN_PORT, SWCLK_TCK_PIN, GPIO_PIN_SET);

    // SWDIO output, idle HIGH
    gpio_init.Pin = SWDIO_OUT_PIN;
    HAL_GPIO_Init(SWDIO_OUT_PIN_PORT, &gpio_init);
    HAL_GPIO_WritePin(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN, GPIO_PIN_SET);

    // SWDIO_IN stays an input
    gpio_init.Pin = SWDIO_IN_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SWDIO_IN_PIN_PORT, &gpio_init);
}

/** Disable SWD I/O Pins. */
__STATIC_INLINE void PORT_OFF(void)
{
    GPIO_InitTypeDef gpio_init =
    {
        .Mode = GPIO_MODE_INPUT,
        .Pull = GPIO_NOPULL,
    };

    gpio_init.Pin = SWCLK_TCK_PIN;
    HAL_GPIO_Init(SWCLK_TCK_PIN_PORT, &gpio_init);

    gpio_init.Pin = SWDIO_OUT_PIN;
    HAL_GPIO_Init(SWDIO_OUT_PIN_PORT, &gpio_init);
}

// SWCLK/TCK I/O pin -------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_SWCLK_TCK_IN(void)
{
    return ((SWCLK_TCK_PIN_PORT->ODR >> SWCLK_TCK_PIN_Bit) & 1);
}

__STATIC_FORCEINLINE void PIN_SWCLK_TCK_SET(void)
{
    SWCLK_TCK_PIN_PORT->BSRR = SWCLK_TCK_PIN;
}

__STATIC_FORCEINLINE void PIN_SWCLK_TCK_CLR(void)
{
    SWCLK_TCK_PIN_PORT->BSRR = (SWCLK_TCK_PIN << 16);
}

// SWDIO/TMS Pin I/O --------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_SWDIO_TMS_IN(void)
{
    return ((SWDIO_IN_PIN_PORT->IDR >> SWDIO_IN_PIN_Bit) & 1);
}

__STATIC_FORCEINLINE void PIN_SWDIO_TMS_SET(void)
{
    SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;
}

__STATIC_FORCEINLINE void PIN_SWDIO_TMS_CLR(void)
{
    SWDIO_OUT_PIN_PORT->BSRR = (SWDIO_OUT_PIN << 16);
}

__STATIC_FORCEINLINE uint32_t PIN_SWDIO_IN(void)
{
    return ((SWDIO_IN_PIN_PORT->IDR >> SWDIO_IN_PIN_Bit) & 1);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT(uint32_t bit)
{
    uint32_t shift = (((~bit) & 1) << 4);
    SWDIO_OUT_PIN_PORT->BSRR = (SWDIO_OUT_PIN << shift);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT_ENABLE(void)
{
    GPIO_InitTypeDef gpio_init =
    {
        .Pin = SWDIO_OUT_PIN,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Speed = GPIO_SPEED_FREQ_HIGH,
    };
    HAL_GPIO_Init(SWDIO_OUT_PIN_PORT, &gpio_init);
    SWDIO_OUT_PIN_PORT->BSRR = (SWDIO_OUT_PIN << 16);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT_DISABLE(void)
{
    GPIO_InitTypeDef gpio_init =
    {
        .Pin = SWDIO_OUT_PIN,
        .Mode = GPIO_MODE_INPUT,
        .Pull = GPIO_NOPULL,
    };
    HAL_GPIO_Init(SWDIO_OUT_PIN_PORT, &gpio_init);
    SWDIO_OUT_PIN_PORT->BSRR = SWDIO_OUT_PIN;
}

// TDI/TDO/nTRST - not available
__STATIC_FORCEINLINE uint32_t PIN_TDI_IN(void)    { return (0); }
__STATIC_FORCEINLINE void     PIN_TDI_OUT(uint32_t bit) { (void)bit; }
__STATIC_FORCEINLINE uint32_t PIN_TDO_IN(void)    { return (0); }
__STATIC_FORCEINLINE uint32_t PIN_nTRST_IN(void)  { return (0); }
__STATIC_FORCEINLINE void     PIN_nTRST_OUT(uint32_t bit) { (void)bit; }

// nRESET - not present on this board
__STATIC_FORCEINLINE uint32_t PIN_nRESET_IN(void)  { return (0); }
__STATIC_FORCEINLINE void     PIN_nRESET_OUT(uint32_t bit) { (void)bit; }

//**************************************************************************************************
/**
\defgroup DAP_Config_LEDs_gr CMSIS-DAP Hardware Status LEDs
*/

__STATIC_INLINE void LED_CONNECTED_OUT(uint32_t bit)
{
    // LED is active low
    if (bit & 1) {
        HAL_GPIO_WritePin(CONNECTED_LED_PORT, CONNECTED_LED_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(CONNECTED_LED_PORT, CONNECTED_LED_PIN, GPIO_PIN_SET);
    }
}

__STATIC_INLINE void LED_RUNNING_OUT(uint32_t bit)
{
    if (bit & 1) {
        HAL_GPIO_WritePin(RUNNING_LED_PORT, RUNNING_LED_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(RUNNING_LED_PORT, RUNNING_LED_PIN, GPIO_PIN_SET);
    }
}

///@}

//**************************************************************************************************
/**
\defgroup DAP_Config_Timestamp_gr CMSIS-DAP Timestamp
*/

__STATIC_INLINE uint32_t TIMESTAMP_GET(void) {
    return (DWT->CYCCNT) / (CPU_CLOCK / TIMESTAMP_CLOCK);
}

///@}

//**************************************************************************************************
/**
\defgroup DAP_Config_Initialization_gr CMSIS-DAP Initialization
*/

__STATIC_INLINE void DAP_SETUP(void)
{
    GPIO_InitTypeDef gpio_init =
    {
        .Pin = CONNECTED_LED_PIN,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };

    PORT_SWD_SETUP();

    HAL_GPIO_Init(CONNECTED_LED_PORT, &gpio_init);
    HAL_GPIO_WritePin(CONNECTED_LED_PORT, CONNECTED_LED_PIN, GPIO_PIN_SET);
}

__STATIC_INLINE uint32_t RESET_TARGET(void)
{
    return (0);
}

///@}

#endif /* __DAP_CONFIG_H__ */
