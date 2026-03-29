/**
 * @file    sdk.c
 * @brief
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2017-2017, ARM Limited, All Rights Reserved
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

#include "hk32f10x.h"
#include "DAP_config.h"
#include "gpio.h"
#include "daplink.h"
#include "util.h"
#include "hal/hk32f1xx_hal.h"
#include "cortex_m.h"
#include "hk32f10x_rcc.h"
#include "hk32f10x_tim.h"

static TIM_TypeDef *s_timer = TIM2;
static volatile uint32_t s_time_count = 0;
static volatile uint16_t s_timer_prescaler = 0;

static uint32_t tim2_clk_div(uint32_t apb1clkdiv)
{
    switch (apb1clkdiv) {
        case RCC_HCLK_Div2:
            return 1;
        case RCC_HCLK_Div4:
            return 2;
        case RCC_HCLK_Div8:
            return 4;
        case RCC_HCLK_Div16:
            return 8;
        default:
            return 1;
    }
}

void sdk_init(void)
{
    /* Enable Flash Interface clock and cache - required for flash operations */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FLITF, ENABLE);
    // RCC_AHBPeriph2ClockCmd(RCC_AHBPeriph2_CACHE, ENABLE);
}

void HAL_InitTick(uint32_t TickPriority)
{
    uint32_t prescaler;
    uint32_t source_clock;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

    source_clock = SystemCoreClock / tim2_clk_div(RCC_HCLK_Div2);
    prescaler = (uint32_t)(source_clock / 4000) - 1;

    s_timer_prescaler = (uint16_t)prescaler;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)prescaler;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_SetCounter(TIM2, 0);
    s_time_count = 0;

    TIM_Cmd(TIM2, ENABLE);
}


void HAL_IncTick(void)
{
}

uint32_t HAL_GetTick(void)
{
    cortex_int_state_t state;
    uint32_t ticks;
    state = cortex_int_get_and_disable();
    ticks = TIM_GetCounter(TIM2) / 4;
    s_time_count += (ticks - s_time_count) & 0x3FFF;
    cortex_int_restore(state);
    return s_time_count;
}

void HAL_SuspendTick(void)
{
    TIM_Cmd(TIM2, DISABLE);
}

void HAL_ResumeTick(void)
{
    TIM_Cmd(TIM2, ENABLE);
}
