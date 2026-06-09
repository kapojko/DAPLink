/**
 * @file    hk32f1xx_hal.h
 * @brief   HAL compatibility layer for HK32F103
 *
 * Provides STM32 HAL-like macros that map to HK32 StdPeriphDriver functions.
 * This allows code written for STM32 HAL to compile for HK32 with minimal changes.
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2026, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HK32F1XX_HAL_H
#define __HK32F1XX_HAL_H

#include "hk32f103xxxxa.h"
#include "hk32f103xxxxa_rcc.h"
#include "hk32f103xxxxa_gpio.h"

#define GPIO_PIN_0                 GPIO_Pin_0
#define GPIO_PIN_1                 GPIO_Pin_1
#define GPIO_PIN_2                 GPIO_Pin_2
#define GPIO_PIN_3                 GPIO_Pin_3
#define GPIO_PIN_4                 GPIO_Pin_4
#define GPIO_PIN_5                 GPIO_Pin_5
#define GPIO_PIN_6                 GPIO_Pin_6
#define GPIO_PIN_7                 GPIO_Pin_7
#define GPIO_PIN_8                 GPIO_Pin_8
#define GPIO_PIN_9                 GPIO_Pin_9
#define GPIO_PIN_10                GPIO_Pin_10
#define GPIO_PIN_11                GPIO_Pin_11
#define GPIO_PIN_12                GPIO_Pin_12
#define GPIO_PIN_13                GPIO_Pin_13
#define GPIO_PIN_14                GPIO_Pin_14
#define GPIO_PIN_15                GPIO_Pin_15

#define __HAL_RCC_GPIOA_CLK_ENABLE()   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
#define __HAL_RCC_GPIOB_CLK_ENABLE()   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE)
#define __HAL_RCC_GPIOC_CLK_ENABLE()   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE)
#define __HAL_RCC_GPIOD_CLK_ENABLE()   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE)
#define __HAL_RCC_GPIOE_CLK_ENABLE()   RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE)

#define __HAL_RCC_GPIOA_CLK_DISABLE()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, DISABLE)
#define __HAL_RCC_GPIOB_CLK_DISABLE()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, DISABLE)
#define __HAL_RCC_GPIOC_CLK_DISABLE()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE)
#define __HAL_RCC_GPIOD_CLK_DISABLE()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, DISABLE)
#define __HAL_RCC_GPIOE_CLK_DISABLE()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, DISABLE)

#define __HAL_AFIO_REMAP_SWJ_NOJTAG() AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | (0x2 << 24);  /* SWJ_CFG = 010: JTAG disabled, SWD enabled */

#define __HAL_RCC_TIM1_CLK_ENABLE()    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE)
#define __HAL_RCC_TIM2_CLK_ENABLE()    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE)
#define __HAL_RCC_TIM3_CLK_ENABLE()    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE)
#define __HAL_RCC_TIM4_CLK_ENABLE()    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE)

#define __HAL_RCC_USART1_CLK_ENABLE()  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE)
#define __HAL_RCC_USART2_CLK_ENABLE()  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE)
#define __HAL_RCC_USART3_CLK_ENABLE()  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE)

#define __HAL_RCC_USB_CLK_ENABLE()     RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE)

#define __HAL_RCC_DMA1_CLK_ENABLE()    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE)

#define __HAL_GPIO_EXTI_GENERATE_SWIT(GPIO_Pin)   EXTI->SWIER |= (GPIO_Pin)

#endif /* __HK32F1XX_HAL_H */
