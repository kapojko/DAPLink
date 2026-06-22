/**
 * @file    Debug.c
 * @brief   USART2-backed debug print and delay helpers for STM32H723VG port.
 *
 * Mirrors the demo's User/Debug.c API (PRINT, DebugPrint, DelayMsFunc,
 * DelayUsFunc). Uses USART2 (PA2/AF7 TX, PA3/AF7 RX, 115200 8N1) matching
 * the demo's MX_USART2_UART_Init. USART2 init is deferred to first use so
 * the SDK doesn't have to wait for it during early boot.
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32h7xx.h"
#include "Debug.h"

static UART_HandleTypeDef s_debug_uart;
static volatile uint8_t s_debug_uart_ready;

#ifdef DEBUG_PRINT
// Override the weak _write() stub in daplink/sdk_stub.c so printf() / vprintf()
// in DebugPrint() actually emits characters via USART2.
int _write(int file, char *ptr, int len)
{
    (void)file;
    if (InitDebug() != HAL_OK) {
        return -1;
    }
    if (HAL_UART_Transmit(&s_debug_uart, (const uint8_t *)ptr, (uint16_t)len, 1000) != HAL_OK) {
        return -1;
    }
    return len;
}
#endif

HAL_StatusTypeDef InitDebug(void)
{
#ifdef DEBUG_PRINT
    GPIO_InitTypeDef gpio_init;
    RCC_PeriphCLKInitTypeDef periph_clk;

    if (s_debug_uart_ready) {
        return HAL_OK;
    }

    // Select D2PCLK1 as USART2 kernel clock (matches demo's MX_USART2_UART_Init).
    periph_clk.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    periph_clk.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
        return HAL_ERROR;
    }

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA2 = USART2_TX, PA3 = USART2_RX, AF7
    gpio_init.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    s_debug_uart.Instance        = USART2;
    s_debug_uart.Init.BaudRate   = 115200;
    s_debug_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_debug_uart.Init.StopBits   = UART_STOPBITS_1;
    s_debug_uart.Init.Parity     = UART_PARITY_NONE;
    s_debug_uart.Init.Mode       = UART_MODE_TX_RX;
    s_debug_uart.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    s_debug_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_debug_uart) != HAL_OK) {
        return HAL_ERROR;
    }

    s_debug_uart_ready = 1;
#endif
    return HAL_OK;
}

void DebugPrint(const char *fmt, ...)
{
#ifdef DEBUG_PRINT
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}

void DelayMsFunc(int ms)
{
    DelayUsFunc(ms * 1000);
}

void DelayUsFunc(int us)
{
    // Use DWT cycle counter (CPU cycles) for high-resolution busy-wait.
    // Assumes DWT->CTRL.CYCCNTENA is enabled by the SDK's HAL_Init path
    // (HAL_Init calls CoreDebug->DEMCR |= TRCENA which enables DWT).
    const uint32_t cycles_per_us = (SystemCoreClock / 1000000U);
    const uint32_t start = DWT->CYCCNT;
    const uint32_t wait_cycles = (uint32_t)us * cycles_per_us;
    while ((DWT->CYCCNT - start) < wait_cycles) {
        // spin
    }
}