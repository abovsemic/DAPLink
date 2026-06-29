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

#include "abov_config.h"
#include "DAP_config.h"
#include "daplink.h"
#include "util.h"
#include "cortex_m.h"
#include "debug.h"
#include "debug_log.h"
#include "hal_timer1.h"
#include "hal_scu_clk.h"
#include "hal_usb.h"

uint32_t time_count;

void GetChipSetInfo(void)
{
    int8_t *pn8ChipInfo = NULL;
    int8_t *pn8ChipCoreInfo = NULL;
    pn8ChipInfo = PRV_CHIPSET_GetInfo();
    pn8ChipCoreInfo = PRV_CHIPSET_GetCoreInfo();
    LOG("************************************************\n\r");
    LOG("- MCU - %s \n",pn8ChipInfo);
    LOG("- Core: ARM %s  \n",pn8ChipCoreInfo);
    LOG("- Communicate via: %s%d - %dbps\n",CONFIG_DEBUG_MODULE_STR,DEBUG_UART_ID,APP_UART_BAUD);
    LOG("- ARM System Core Clock = %d\n",SystemCoreClock);
    LOG("- LED GPIO Blinking Example\n");
    LOG("************************************************\n\r");
}

void sdk_init()
{
    SCUCLK_MCLK_CFG_t tMClkCfg = 
    {
        //.eMClk = SCUCLK_SRC_HSE,
        .eMClk = SCUCLK_SRC_HSI,			
        .ePreMClkDiv = SCUCLK_DIV_NONE,
        .ePostMClkDiv = SCUCLK_DIV_NONE
    };

    PRV_PORT_Init();

    (void)HAL_SCU_CLK_SetSrcEnable(SCUCLK_SRC_HSE, true);
    (void)HAL_SCU_CLK_SetSrcEnable(SCUCLK_SRC_HSI, true);
    (void)HAL_SCU_CLK_SetMClk(&tMClkCfg);

    /* PLL Clock 48MHz */
    SCUCLK_PLL_CFG_t tPllCfg = 
    {
        .eSrc = SCUCLK_PLL_SRC_HSI,
        .eSrcDiv = SCUCLK_DIV_4,
        .un8PreDiv = 3,
        .un8PostDiv1 = 1,
        .un8PostDiv2 = 1,
        .un8OutDiv = 23,
        .ePllMode = SCUCLK_PLL_MODE_VCO
    };		

    /* Set PLL 48MHz */
    (void)HAL_SCU_CLK_SetPLLConfig(true, &tPllCfg);

    tMClkCfg.eMClk = SCUCLK_SRC_PLL;
    tMClkCfg.ePreMClkDiv = SCUCLK_DIV_NONE;
    tMClkCfg.ePostMClkDiv = SCUCLK_DIV_NONE;
				
    /* Set MCLK as a PLL 48MHz */
    (void)HAL_SCU_CLK_SetMClk(&tMClkCfg);

    /* serial init */
    HAL_PCU_SetAltMode((PCU_ID_e)DEBUG_PORT_ID,(PCU_PIN_ID_e)DEBUG_TX_PORT_ID,(PCU_ALT_e)DEBUG_TX_ALT_ID); /* For Debug */
    HAL_PCU_SetAltMode((PCU_ID_e)DEBUG_PORT_ID,(PCU_PIN_ID_e)DEBUG_RX_PORT_ID,(PCU_ALT_e)DEBUG_RX_ALT_ID); /* For Debug */
    Debug_Init();
    GetChipSetInfo();
		
    LOG("End of SDK Init\n");


#if 1 //ENABLE_CLOCK_OUTPUT
    (void)HAL_PCU_SetAltMode(PCU_ID_B, PCU_PIN_ID_12, PCU_ALT_3);
    (void)HAL_SCU_CLK_SetOutput(SCUCLK_SRC_MCLK, 0, true);
#endif

}

HAL_ERR_e HAL_InitTick(uint32_t TickPriority)
{
    /*
        to set playtime.
        PCLK : 48
        TIMER1 Count = 4KHz
    */
    /* 1 count = 0.25ms */
    TIMER1_CLK_CFG_t tTimer1ClkCfg = {
        .eClk = TIMER1_CLK_PCLK,
        .un16PreScale = 11999, /* 48MHz/(11999+1) = 4000 */ 
    };

    TIMER1_CFG_t tTimer1Cfg = {
        .eMode = TIMER1_MODE_PERIODIC,
        .bIntrEnable = false,
        .bOVFIntrEnable = false,
        .utData.tGRD.un16DataA = 65535,
        .utData.tGRD.un16DataB = 65535,
    };

    HAL_TIMER1_Init(TIMER1_ID_0);
    HAL_TIMER1_SetClkConfig(TIMER1_ID_0, &tTimer1ClkCfg);
    HAL_TIMER1_SetConfig(TIMER1_ID_0, &tTimer1Cfg);
    HAL_TIMER1_SetIRQ(TIMER1_ID_0, TIMER1_OPS_POLL, NULL, NULL, 0);
    HAL_TIMER1_Start(TIMER1_ID_0);

    LOG("Tick Log done!\n");

    return HAL_ERR_OK;
}


void HAL_IncTick(void)
{
    // Do nothing
}

uint32_t HAL_GetTick(void)
{
    uint32_t ticks = 0;
    HAL_TIMER1_GetData(TIMER1_ID_0, TIMER1_DATA_CNT, &ticks);
    /* 1 tick = 1ms */
    time_count += (ticks / 4 - time_count) & 0x3FFF;
    return time_count;
}

void HAL_SuspendTick(void)
{
    HAL_TIMER1_SetPause(TIMER1_ID_0, true);
}

void HAL_ResumeTick(void)
{
    HAL_TIMER1_SetPause(TIMER1_ID_0, false);
}
