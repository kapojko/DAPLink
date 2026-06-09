/**
 * @file    gpio.c
 * @brief
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
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "hk32f103xxxxa.h"
#include "DAP_config.h"
#include "gpio.h"
#include "daplink.h"
#include "util.h"
#include "settings.h"
#include "hal/hk32f1xx_hal.h"
#include "hk32f103xxxxa_rcc.h"
#include "hk32f103xxxxa_gpio.h"
#include "hk32f103xxxxa_tim.h"

static void busy_wait(uint32_t cycles)
{
    volatile uint32_t i;
    i = cycles;

    while (i > 0) {
        i--;
    }
}

static void output_clock_enable(void)
{
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;
    uint32_t period;
    uint32_t source_clock;

    source_clock = SystemCoreClock;
    period = (uint32_t)(source_clock / 8000000) - 1;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
    TIM_OCInitStructure.TIM_Pulse = period / 2;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Set;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

void gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_AFIO, ENABLE);

    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    USB_CONNECT_PORT_ENABLE();
    USB_CONNECT_OFF();
    GPIO_InitStructure.GPIO_Pin = USB_CONNECT_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(USB_CONNECT_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = RUNNING_LED_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(RUNNING_LED_PORT, &GPIO_InitStructure);
    GPIO_SetBits(RUNNING_LED_PORT, RUNNING_LED_PIN);

    GPIO_InitStructure.GPIO_Pin = CONNECTED_LED_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CONNECTED_LED_PORT, &GPIO_InitStructure);
    GPIO_SetBits(CONNECTED_LED_PORT, CONNECTED_LED_PIN);

    GPIO_InitStructure.GPIO_Pin = PIN_CDC_LED;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(PIN_CDC_LED_PORT, &GPIO_InitStructure);
    GPIO_SetBits(PIN_CDC_LED_PORT, PIN_CDC_LED);

    GPIO_InitStructure.GPIO_Pin = PIN_MSC_LED;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(PIN_MSC_LED_PORT, &GPIO_InitStructure);
    GPIO_SetBits(PIN_MSC_LED_PORT, PIN_MSC_LED);

    GPIO_InitStructure.GPIO_Pin = nRESET_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(nRESET_PIN_PORT, &GPIO_InitStructure);

    GPIO_SetBits(POWER_EN_PIN_PORT, POWER_EN_PIN);
    GPIO_InitStructure.GPIO_Pin = POWER_EN_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(POWER_EN_PIN_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(POWER_EN_PIN_PORT, POWER_EN_PIN);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    output_clock_enable();

    // NOTE: disable config_ram initial hold in bootloader,
    // only reset button
    config_ram_set_hold_in_bl(false);

    busy_wait(1000000);
}

void gpio_set_hid_led(gpio_led_state_t state)
{
    if (state) {
        GPIO_ResetBits(PIN_HID_LED_PORT, PIN_HID_LED);
    } else {
        GPIO_SetBits(PIN_HID_LED_PORT, PIN_HID_LED);
    }
}

void gpio_set_cdc_led(gpio_led_state_t state)
{
    if (state) {
        GPIO_ResetBits(PIN_CDC_LED_PORT, PIN_CDC_LED);
    } else {
        GPIO_SetBits(PIN_CDC_LED_PORT, PIN_CDC_LED);
    }
}

void gpio_set_msc_led(gpio_led_state_t state)
{
    if (state) {
        GPIO_ResetBits(PIN_MSC_LED_PORT, PIN_MSC_LED);
    } else {
        GPIO_SetBits(PIN_MSC_LED_PORT, PIN_MSC_LED);
    }
}

uint8_t gpio_get_reset_btn_no_fwrd(void)
{
    return (nRESET_PIN_PORT->IDR & nRESET_PIN) ? 0 : 1;
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
}

void gpio_set_board_power(bool powerEnabled)
{
}
