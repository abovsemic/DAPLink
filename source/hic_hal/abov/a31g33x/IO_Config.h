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

/* PB7 */
#define CONNECTED_LED_PORT           PCU_ID_B
#define CONNECTED_LED_PIN            PCU_PIN_ID_7
#define CONNECTED_LED_PIN_Bit        0

//When bootloader, disable the target port(not used)
//#define POWER_EN_PIN_PORT            GPIOB
//#define POWER_EN_PIN                 GPIO_PIN_15
//#define POWER_EN_Bit                 15

// nRESET OUT Pin
#define nRESET_PIN_PORT              PCU_ID_B
#define nRESET_PIN                   PCU_PIN_ID_0
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
#define JTAG_TDO_PIN_PORT           PCU_ID_B
#define JTAG_TDO_PIN                PCU_PIN_ID_2
#define JTAG_TDO_PIN_Bit            0

#define JTAG_TDI_PIN_PORT           PCU_ID_B
#define JTAG_TDI_PIN                PCU_PIN_ID_1
#define JTAG_TDI_PIN_Bit            0

//LEDs
//USB status LED
//#define RUNNING_LED_PORT             GPIOB
//#define RUNNING_LED_PIN              GPIO_PIN_12
//#define RUNNING_LED_Bit              12

/* PB10 */
#define PIN_HID_LED_PORT             PCU_ID_B
#define PIN_HID_LED                  PCU_PIN_ID_13
#define PIN_HID_LED_Bit              0

//#define PIN_CDC_LED_PORT             GPIOB
//#define PIN_CDC_LED                  GPIO_PIN_13
//#define PIN_CDC_LED_Bit              13

//#define PIN_MSC_LED_PORT             GPIOB
//#define PIN_MSC_LED                  GPIO_PIN_12
//#define PIN_MSC_LED_Bit              12


#endif
