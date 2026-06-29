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

#include "abov_config.h"
#include "hal_pcu.h"
#include "DAP_config.h"
#include "gpio.h"
#include "daplink.h"
#include "util.h"

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
    return;
}

void gpio_init(void)
{

    /* HID LED */
    HAL_PCU_SetInOutMode((PCU_ID_e)PIN_HID_LED_PORT, (PCU_PIN_ID_e)PIN_HID_LED, PCU_INOUT_OUTPUT_PUSH_PULL);
    HAL_PCU_SetOutputBit((PCU_ID_e)PIN_HID_LED_PORT, (PCU_PIN_ID_e)PIN_HID_LED, PCU_OUTPUT_BIT_CLEAR);

    busy_wait(1000000);
}

void gpio_set_hid_led(gpio_led_state_t state)
{
    // LED is active low
    HAL_PCU_SetOutputBit((PCU_ID_e)PIN_HID_LED_PORT, (PCU_PIN_ID_e)PIN_HID_LED, state ? PCU_OUTPUT_BIT_CLEAR : PCU_OUTPUT_BIT_SET);
}

void gpio_set_cdc_led(gpio_led_state_t state)
{
    // LED is active low
}

void gpio_set_msc_led(gpio_led_state_t state)
{
    // LED is active low
}

uint8_t gpio_get_reset_btn_no_fwrd(void)
{
    PCU_PORT_e eInput;
    HAL_PCU_GetInputValue((PCU_ID_e)nRESET_PIN_PORT, (PCU_PIN_ID_e)nRESET_PIN, &eInput);
    return (eInput == PCU_PORT_HIGH) ? 0 : 1;
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
    // Do nothing - reset is forwarded in gpio_get_sw_reset
}

void gpio_set_board_power(bool powerEnabled)
{
}
