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


#include "abov_config.h"
#include "hal_pcu.h"
#include "hal_usart.h"

#include "string.h"
#include "uart.h"

#include "gpio.h"
#include "util.h"
#include "circ_buf.h"
#include "IO_Config.h"

// For usart

#define USART_TX_PORT                 PCU_ID_A
#define USART_TX_PIN                  PCU_PIN_ID_9

#define USART_RX_PORT                 PCU_ID_A
#define USART_RX_PIN                  PCU_PIN_ID_10

#define RX_OVRF_MSG         "<DAPLink:Overflow>\n"
#define RX_OVRF_MSG_SIZE    (sizeof(RX_OVRF_MSG) - 1)
#define BUFFER_SIZE         (512)

circ_buf_t write_buffer;
uint8_t write_buffer_data[BUFFER_SIZE];
circ_buf_t read_buffer;
uint8_t read_buffer_data[BUFFER_SIZE];

uint8_t rbuf = 0, tbuf = 0; 

static UART_Configuration configuration = {
    .Baudrate = 38400,
    .DataBits = UART_DATA_BITS_8,
    .Parity = (UART_Parity)UART_PARITY_NONE,
    .StopBits = UART_STOP_BITS_1,
    .FlowControl = UART_FLOW_CONTROL_NONE,
};

extern uint32_t SystemCoreClock;

void CDC_USART_IRQn_Handler(uint32_t un32Event, void *pContext)
{
    if (un32Event & USART_EVENT_RX_DONE) {
        uint8_t dat = rbuf;
        uint32_t free = circ_buf_count_free(&read_buffer);
        if (free > RX_OVRF_MSG_SIZE) {
            circ_buf_push(&read_buffer, dat);
        } else if (RX_OVRF_MSG_SIZE == free) {
            circ_buf_write(&read_buffer, (uint8_t*)RX_OVRF_MSG, RX_OVRF_MSG_SIZE);
        } else {
            // Drop character
        }
        HAL_USART_Receive(USART_ID_1, &rbuf, 1, false);
    }

    if (un32Event & USART_EVENT_TX_DONE) {
        if (circ_buf_count_used(&write_buffer) > 0) {
            tbuf = circ_buf_pop(&write_buffer);
            HAL_USART_Receive(USART_ID_1, &tbuf, 1, false);
        } 
    }
}


static void clear_buffers(void)
{
    circ_buf_init(&write_buffer, write_buffer_data, sizeof(write_buffer_data));
    circ_buf_init(&read_buffer, read_buffer_data, sizeof(read_buffer_data));
}

int32_t uart_initialize(void)
{
    HAL_PCU_SetAltMode((PCU_ID_e)USART_TX_PORT, (PCU_PIN_ID_e)USART_TX_PIN, PCU_ALT_2);
    HAL_PCU_SetAltMode((PCU_ID_e)USART_RX_PORT, (PCU_PIN_ID_e)USART_RX_PIN, PCU_ALT_2);

    HAL_USART_Init(USART_ID_1);

    return 1;
}

int32_t uart_uninitialize(void)
{
    HAL_USART_Uninit(USART_ID_1);
    clear_buffers();
    return 1;
}

int32_t uart_reset(void)
{
    HAL_USART_Abort(USART_ID_1);
    clear_buffers();
    return 1;
}

int32_t uart_set_configuration(UART_Configuration *config)
{

    USART_CFG_t tUsartCfg;


    HAL_USART_Abort(USART_ID_1);

    HAL_USART_Uninit(USART_ID_1);

    HAL_USART_Init(USART_ID_1);
    clear_buffers();

    tUsartCfg.eMode = USART_MODE_UART;

    // Parity bits
    configuration.Parity = config->Parity;
    if(config->Parity == UART_PARITY_ODD) {
        tUsartCfg.tCfg.tUart.eParity = USART_PARITY_ODD;
    } else if(config->Parity == UART_PARITY_EVEN) {
        tUsartCfg.tCfg.tUart.eParity = USART_PARITY_EVEN;
    } else if(config->Parity == UART_PARITY_NONE) {
        tUsartCfg.tCfg.tUart.eParity = USART_PARITY_NONE;
    } else {   //Other not support
        tUsartCfg.tCfg.tUart.eParity = USART_PARITY_NONE;
        configuration.Parity = UART_PARITY_NONE;
    }

    // stop bits
    configuration.StopBits = config->StopBits;
    if(config->StopBits == UART_STOP_BITS_2) {
        tUsartCfg.tCfg.tUart.eStop = USART_STOP_2;
    } else if(config->StopBits == UART_STOP_BITS_1_5) {
        tUsartCfg.tCfg.tUart.eStop = USART_STOP_2;
        configuration.StopBits = UART_STOP_BITS_2;
    } else if(config->StopBits == UART_STOP_BITS_1) {
        tUsartCfg.tCfg.tUart.eStop = USART_STOP_1;
    } else {
        tUsartCfg.tCfg.tUart.eStop = USART_STOP_1;
        configuration.StopBits = UART_STOP_BITS_1;
    }

    //Only 8 bit support
    configuration.DataBits = UART_DATA_BITS_8;
    tUsartCfg.tCfg.tUart.eData = USART_DATA_8;

    // No flow control
    configuration.FlowControl = UART_FLOW_CONTROL_NONE;
    
    // Specified baudrate
    configuration.Baudrate = config->Baudrate;
    tUsartCfg.un32BaudRate = config->Baudrate;

    HAL_USART_SetConfig(USART_ID_1, &tUsartCfg);

    HAL_USART_SetIRQ(USART_ID_1, USART_OPS_INTR, CDC_USART_IRQn_Handler, NULL, 15);

    HAL_USART_Receive(USART_ID_1, &rbuf, 1, false);

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
    return circ_buf_count_free(&write_buffer);
}

int32_t uart_write_data(uint8_t *data, uint16_t size)
{
    uint32_t cnt = circ_buf_write(&write_buffer, data, size);

    if (circ_buf_count_used(&write_buffer) > 0) {
        tbuf = circ_buf_pop(&write_buffer);
        HAL_USART_Transmit(USART_ID_1, &tbuf, 1, false);
    }

    return cnt;
}

int32_t uart_read_data(uint8_t *data, uint16_t size)
{
    return circ_buf_read(&read_buffer, data, size);
}

