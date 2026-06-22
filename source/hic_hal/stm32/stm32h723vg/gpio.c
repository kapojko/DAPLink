/**
 * @file    gpio.c
 * @brief   Board-level GPIO initialization for STM32H723VG.
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

#include "stm32h7xx.h"
#include "DAP_config.h"
#include "gpio.h"
#include "daplink.h"
#include "util.h"

#define GPIO_INIT_STABILIZATION_DELAY_MS    (100)

void gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIO ports used on this board
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // LEDs (all active-low - set HIGH initially to keep them off)
    HAL_GPIO_WritePin(CONNECTED_LED_PORT, CONNECTED_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RUNNING_LED_PORT, RUNNING_LED_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = CONNECTED_LED_PIN | RUNNING_LED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(EXTRA_LED_PORT, EXTRA_LED_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = EXTRA_LED_PIN;
    HAL_GPIO_Init(EXTRA_LED_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(PIN_HID_LED_PORT, PIN_HID_LED, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_CDC_LED_PORT, PIN_CDC_LED, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PIN_MSC_LED_PORT, PIN_MSC_LED, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = PIN_HID_LED | PIN_CDC_LED | PIN_MSC_LED;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // Power control (default off)
    if (daplink_is_interface()) {
        HAL_GPIO_WritePin(POWER_EN_PIN_PORT, POWER_EN_PIN, GPIO_PIN_RESET);
        GPIO_InitStruct.Pin = POWER_EN_PIN;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(POWER_EN_PIN_PORT, &GPIO_InitStruct);
    }

    // SWD pins - SWCLK and SWDIO_OUT as push-pull outputs, SWDIO_IN as input
    HAL_GPIO_WritePin(SWCLK_TCK_PIN_PORT, SWCLK_TCK_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SWDIO_OUT_PIN_PORT, SWDIO_OUT_PIN, GPIO_PIN_SET);
    GPIO_InitStruct.Pin = SWCLK_TCK_PIN | SWDIO_OUT_PIN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SWCLK_TCK_PIN_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SWDIO_IN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SWDIO_IN_PIN_PORT, &GPIO_InitStruct);

    // Buttons (BTN_BOOT on PB8, BTN_CONF on PB9). Configure as EXTI rising
    // edges to match the demo's MX_GPIO_Init. GPIOB is already clocked above.
    GPIO_InitStruct.Pin = BTN_BOOT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BTN_BOOT_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BTN_CONF_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(BTN_CONF_PORT, &GPIO_InitStruct);

    // EXTI9_5 IRQ - shared by PB8 and PB9. The handler below just clears
    // the pending bit via HAL_GPIO_EXTI_IRQHandler.
    HAL_NVIC_SetPriority(BTN_BOOT_EXTI_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(BTN_BOOT_EXTI_IRQn);

    // Let the voltage rails stabilize.
    HAL_Delay(GPIO_INIT_STABILIZATION_DELAY_MS);
}

/* Shared EXTI9_5 IRQ for BTN_BOOT (PB8) and BTN_CONF (PB9). The HAL
 * clears the EXTI pending bit and invokes HAL_GPIO_EXTI_Callback (a
 * weak no-op by default). DAPLink polls button state via IDR, so the
 * callback itself does not need to do anything. */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BTN_BOOT_PIN);
    HAL_GPIO_EXTI_IRQHandler(BTN_CONF_PIN);
}

void gpio_set_hid_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_GPIO_WritePin(PIN_HID_LED_PORT, PIN_HID_LED, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void gpio_set_cdc_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_GPIO_WritePin(PIN_CDC_LED_PORT, PIN_CDC_LED, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void gpio_set_msc_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_GPIO_WritePin(PIN_MSC_LED_PORT, PIN_MSC_LED, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool reset_button_pressed(void)
{
    // BTN_CONF (PB9) acts as the alternate reset-button source
    return (BTN_CONF_PORT->IDR & BTN_CONF_PIN) ? 1 : 0;
}

uint8_t gpio_get_reset_btn_no_fwrd(void)
{
    // Active-high on this board: pressed = 1
    return (BTN_CONF_PORT->IDR & BTN_CONF_PIN) ? 1 : 0;
}

uint8_t gpio_get_reset_btn_fwrd(void)
{
    return 0;
}

uint8_t GPIOGetButtonState(void)
{
    return 0;
}

void target_forward_reset(bool assert_reset)
{
    // This board has no target nRESET pin.
    (void)assert_reset;
}

void gpio_set_board_power(bool powerEnabled)
{
    HAL_GPIO_WritePin(POWER_EN_PIN_PORT, POWER_EN_PIN,
                      powerEnabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_30ms_hook(void)
{
    static uint8_t btn_prev = 0;
    uint8_t btn_cur = (BTN_CONF_PORT->IDR & BTN_CONF_PIN) ? 1 : 0;

    // Toggle power on rising edge of BTN_CONF press
    if (btn_cur && !btn_prev) {
        HAL_GPIO_TogglePin(POWER_EN_PIN_PORT, POWER_EN_PIN);
    }

    btn_prev = btn_cur;
}
