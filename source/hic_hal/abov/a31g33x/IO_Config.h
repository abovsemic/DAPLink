/**
 * @file    IO_Config.h
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

#ifndef __IO_CONFIG_H__
#define __IO_CONFIG_H__

#include "abov_config.h"
#include "hal_pcu.h"
#include "compiler.h"
#include "daplink.h"

COMPILER_ASSERT(DAPLINK_HIC_ID == DAPLINK_HIC_ID_STM32F103XB);

//USB control pin
#define USB_CONNECT_PORT_ENABLE()    /* Not Used */
#define USB_CONNECT_PORT_DISABLE()   /* Not Used */
#define USB_CONNECT_PORT             /* Not Used */
#define USB_CONNECT_PIN              /* Not Used */
#define USB_CONNECT_ON()             /* Not Used */
#define USB_CONNECT_OFF()            /* Not Used */

//Connected LED

/* CONNECTED LED (LED1) */
#define CONNECTED_LED_PORT           PCU_ID_A
#define CONNECTED_LED_PIN            PCU_PIN_ID_3
#define CONNECTED_LED_PIN_Bit        0

// nRESET OUT Pin
#define nRESET_PIN_PORT              PCU_ID_A
#define nRESET_PIN                   PCU_PIN_ID_5
#define nRESET_PIN_Bit               0

//SWD 
#define SWCLK_TCK_PIN_PORT           PCU_ID_B
#define SWCLK_TCK_PIN                PCU_PIN_ID_10
#define SWCLK_TCK_PIN_Bit            0

#define SWDIO_OUT_PIN_PORT           PCU_ID_B
#define SWDIO_OUT_PIN                PCU_PIN_ID_11
#define SWDIO_OUT_PIN_Bit            0

#define SWDIO_IN_PIN_PORT            PCU_ID_B
#define SWDIO_IN_PIN                 PCU_PIN_ID_11
#define SWDIO_IN_PIN_Bit             0

//JTAG
#define JTAG_TDO_PIN_PORT            PCU_ID_A
#define JTAG_TDO_PIN                 PCU_PIN_ID_1    // TDO/SWO: PA1 (AF2 = RXD13 for SWO, Driver_USART_a31g33x.c)
#define JTAG_TDO_PIN_Bit             0

#define JTAG_TDI_PIN_PORT            PCU_ID_A
#define JTAG_TDI_PIN                 PCU_PIN_ID_7
#define JTAG_TDI_PIN_Bit             0

/* HID LED (LED0) */
#define PIN_HID_LED_PORT             PCU_ID_A
#define PIN_HID_LED                  PCU_PIN_ID_2
#define PIN_HID_LED_Bit              0


/* CDC LED (LED0) */
#define PIN_CDC_LED_PORT             PCU_ID_A
#define PIN_CDC_LED                  PCU_PIN_ID_2
#define PIN_CDC_LED_Bit              0

#if (CONFIG_DEBUG == 1) && (DEBUG_UART_ID == 0)
#error "Debug log on USART10 uses PA2/PA3 = LED0/LED1: set TARGET_BOARD_DEBUG_ENABLE to 0"
#endif

#endif
