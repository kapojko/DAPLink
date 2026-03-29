/**
 * @file    uart.c
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

#include "string.h"

#include "hk32f10x.h"
#include "uart.h"
#include "gpio.h"
#include "util.h"
#include "circ_buf.h"
#include "IO_Config.h"
#include "hk32f10x_rcc.h"
#include "hk32f10x_gpio.h"
#include "hk32f10x_usart.h"
#include "hk32f10x_dma.h"

#define CDC_UART                     USART2
#define CDC_UART_ENABLE()            RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE)
#define CDC_UART_DISABLE()           RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE)
#define CDC_UART_IRQn                USART2_IRQn
#define CDC_UART_IRQn_Handler        USART2_IRQHandler
#define CDC_UART_DMA_TX_CHANNEL      DMA1_Channel7

#define UART_PINS_PORT_ENABLE()      RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)
#define UART_PINS_PORT_DISABLE()     RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, DISABLE)

#define UART_TX_PORT                 GPIOA
#define UART_TX_PIN                  GPIO_Pin_2

#define UART_RX_PORT                 GPIOA
#define UART_RX_PIN                  GPIO_Pin_3

#define RX_OVRF_MSG         "<DAPLink:Overflow>\n"
#define RX_OVRF_MSG_SIZE    (sizeof(RX_OVRF_MSG) - 1)
#define BUFFER_SIZE         (512)

circ_buf_t write_buffer;
uint8_t write_buffer_data[BUFFER_SIZE];
circ_buf_t read_buffer;
uint8_t read_buffer_data[BUFFER_SIZE];

static volatile uint8_t transfer_in_progress = 0;

static UART_Configuration configuration = {
    .Baudrate = 9600,
    .DataBits = UART_DATA_BITS_8,
    .Parity = UART_PARITY_NONE,
    .StopBits = UART_STOP_BITS_1,
    .FlowControl = UART_FLOW_CONTROL_NONE,
};

extern uint32_t SystemCoreClock;



static void clear_buffers(void)
{
    circ_buf_init(&write_buffer, write_buffer_data, sizeof(write_buffer_data));
    circ_buf_init(&read_buffer, read_buffer_data, sizeof(read_buffer_data));
}

int32_t uart_initialize(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_StructInit(&GPIO_InitStructure);

    CDC_UART_ENABLE();
    UART_PINS_PORT_ENABLE();
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(UART_RX_PORT, &GPIO_InitStructure);

    NVIC_EnableIRQ(CDC_UART_IRQn);

    DMA_InitTypeDef DMA_InitStructure;
    DMA_StructInit(&DMA_InitStructure);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&CDC_UART->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)write_buffer_data;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(CDC_UART_DMA_TX_CHANNEL, &DMA_InitStructure);

    DMA_ITConfig(CDC_UART_DMA_TX_CHANNEL, DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(DMA1_Channel7_IRQn);

    return 1;
}

int32_t uart_uninitialize(void)
{
    USART_ITConfig(CDC_UART, USART_IT_TXE, DISABLE);
    USART_ITConfig(CDC_UART, USART_IT_RXNE, DISABLE);
    clear_buffers();
    return 1;
}

int32_t uart_reset(void)
{
    USART_ITConfig(CDC_UART, USART_IT_TXE, DISABLE);
    USART_ITConfig(CDC_UART, USART_IT_RXNE, DISABLE);
    clear_buffers();
    return 1;
}

int32_t uart_set_configuration(UART_Configuration *config)
{
    USART_InitTypeDef USART_InitStructure;
    USART_StructInit(&USART_InitStructure);

    configuration.Parity = config->Parity;
    configuration.StopBits = config->StopBits;
    configuration.DataBits = UART_DATA_BITS_8;
    configuration.Baudrate = config->Baudrate;
    configuration.FlowControl = UART_FLOW_CONTROL_NONE;

    USART_InitStructure.USART_BaudRate = config->Baudrate;

    if(config->Parity == UART_PARITY_ODD) {
        USART_InitStructure.USART_Parity = USART_Parity_Odd;
    } else if(config->Parity == UART_PARITY_EVEN) {
        USART_InitStructure.USART_Parity = USART_Parity_Even;
    } else {
        USART_InitStructure.USART_Parity = USART_Parity_No;
        configuration.Parity = UART_PARITY_NONE;
    }

    if(config->StopBits == UART_STOP_BITS_2) {
        USART_InitStructure.USART_StopBits = USART_StopBits_2;
    } else {
        USART_InitStructure.USART_StopBits = USART_StopBits_1;
        configuration.StopBits = UART_STOP_BITS_1;
    }

    if (USART_InitStructure.USART_Parity == USART_Parity_Odd || USART_InitStructure.USART_Parity == USART_Parity_Even) {
        USART_InitStructure.USART_WordLength = USART_WordLength_9b;
    } else {
        USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    }

    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_ITConfig(CDC_UART, USART_IT_TXE, DISABLE);
    USART_ITConfig(CDC_UART, USART_IT_RXNE, DISABLE);
    clear_buffers();

    USART_DeInit(CDC_UART);

    USART_Init(CDC_UART, &USART_InitStructure);
    USART_DMACmd(CDC_UART, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(CDC_UART, ENABLE);

    USART_ITConfig(CDC_UART, USART_IT_RXNE, ENABLE);

    return 1;
}

int32_t uart_get_configuration(UART_Configuration *config)
{
    config->Baudrate = configuration.Baudrate;
    config->DataBits = configuration.DataBits;
    config->Parity   = configuration.Parity;
    config->StopBits = configuration.StopBits;
    config->FlowControl = UART_FLOW_CONTROL_NONE;

    return 1;
}

void uart_set_control_line_state(uint16_t ctrl_bmp)
{
}

int32_t uart_write_free(void)
{
    return (transfer_in_progress) ? 0 : sizeof(write_buffer_data);
    // return circ_buf_count_free(&write_buffer);
}

int32_t uart_write_data(uint8_t *data, uint16_t size)
{
    if (size == 0) {
        return 0;
    }

    // Wait previous transmission to complete
    while (transfer_in_progress);

    // Copy data to buffer
    memcpy(write_buffer_data, data, size);

    // Start DMA Transfer
    transfer_in_progress = 1;
    DMA_SetCurrDataCounter(CDC_UART_DMA_TX_CHANNEL, size);
    DMA_Cmd(CDC_UART_DMA_TX_CHANNEL, ENABLE);

    return size;

#if 0
    uint32_t cnt;

    if (!transfer_in_progress) {
        // Push first byte manually to trigger the transfer
        transfer_in_progress = 1;
        cnt = circ_buf_write(&write_buffer, data + 1, size - 1) + 1;
        USART_SendData(CDC_UART, data[0]);
        USART_ITConfig(CDC_UART, USART_IT_TXE, ENABLE);
    } else {
        // Transfer already in progress, just push data
        cnt = circ_buf_write(&write_buffer, data, size);
        USART_ITConfig(CDC_UART, USART_IT_TXE, ENABLE);
    }

    return cnt;
#endif
}

int32_t uart_read_data(uint8_t *data, uint16_t size)
{
    return circ_buf_read(&read_buffer, data, size);
}

void CDC_UART_IRQn_Handler(void)
{
    if (USART_GetITStatus(CDC_UART, USART_IT_RXNE) == SET) {
        uint8_t dat = (uint8_t)USART_ReceiveData(CDC_UART);
        uint32_t free = circ_buf_count_free(&read_buffer);
        if (free > RX_OVRF_MSG_SIZE) {
            circ_buf_push(&read_buffer, dat);
        } else if (RX_OVRF_MSG_SIZE == free) {
            circ_buf_write(&read_buffer, (uint8_t*)RX_OVRF_MSG, RX_OVRF_MSG_SIZE);
        }
    }

    // if (USART_GetITStatus(CDC_UART, USART_IT_TXE) == SET) {
    //     if (circ_buf_count_used(&write_buffer) > 0) {
    //         USART_SendData(CDC_UART, circ_buf_pop(&write_buffer));
    //     } else {
    //         USART_ITConfig(CDC_UART, USART_IT_TXE, DISABLE);
    //         transfer_in_progress = 0;
    //     }
    // }
}

void DMA1_Channel7_IRQHandler(void) {
    if (DMA_GetITStatus(DMA1_IT_TC7) == SET) {
        DMA_ClearITPendingBit(DMA1_IT_TC7);
        DMA_Cmd(CDC_UART_DMA_TX_CHANNEL, DISABLE);
        transfer_in_progress = 0;
    }
}