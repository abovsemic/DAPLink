/*
 * Copyright (c) 2004-2016 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*----------------------------------------------------------------------------
*      RL-ARM - USB
*----------------------------------------------------------------------------
*      Name:    usbd_STM32F103.c
*      Purpose: Hardware Layer module for ST STM32F103
*      Rev.:    V4.70
*---------------------------------------------------------------------------*/

/* Double Buffering is not supported                                         */

#include <rl_usb.h>
#include "abov_config.h"
#include "hal_pcu.h"
#include "hal_usb.h"
#include "IO_Config.h"
#include "cortex_m.h"
#include "string.h"
#include "hal_usb.h"
#include "usbd_core.h"

#include "debug_log.h"

#define __NO_USB_LIB_C
#include "usb_config.c"


#define USB_EVT_QUEUE_SIZE    16

typedef struct {
    uint32_t un32Event;
    USB_EVENT_e eUsbEvt;
} USB_EvtItem_t;

typedef struct {
    USB_EvtItem_t tItems[USB_EVT_QUEUE_SIZE];
    uint32_t un32Cnt;
    volatile uint8_t u8Head;
    volatile uint8_t u8Tail;
} USB_EvtQueue_t;

typedef enum {
    EP0_STAGE_IDLE = 0,
    EP0_STAGE_SETUP,
    EP0_STAGE_DATA_OUT,
    EP0_STAGE_DATA_IN,
    EP0_STAGE_STATUS_OUT,
    EP0_STAGE_STATUS_IN,
} ep0_stage_t;

static ep0_stage_t ep0_stage = EP0_STAGE_IDLE;

static volatile USB_EvtQueue_t g_tUsbEvtQ = { .un32Cnt = 0, .u8Head = 0, .u8Tail = 0 };

static void USB_HW_IRQHandler(uint32_t un32Event, void *pContext);

/* HCLK is generated from HSI by default */
#define TC_SYSTICK_1MS      1000

extern uint32_t SystemCoreClock;
static volatile uint32_t s_un32SysTimerVal=0;

static uint8_t s_un8FAddr = 0;
static bool s_bFaddr = false;

static volatile uint8_t ep0_setup_processing = 0;
static volatile uint8_t ep0_h2d_candidate    = 0;
static volatile uint8_t ep0_setup_stalled    = 0;
static volatile uint8_t ep0_status_in_zlp    = 0;
static volatile uint8_t ep0_wait_data_out    = 0;

#define DEBUG                  0

#if (DEBUG == 1)
#define USB_LOG    LOG
#else
#define USB_LOG(...)
#endif

/*
 *  USB Device Interrupt enable
 *   Called by USBD_Init to enable the USB Interrupt
 *    Return Value:    None
 */

#ifdef __RTX
void __svc(1) USBD_IntrEna(void);
void __SVC_1(void)
{
#else
void          USBD_IntrEna(void)
{
#endif
    HAL_USB_SetIRQ(USB_ID_0, USB_OPS_INTR, USB_HW_IRQHandler, NULL, 3, false);
}

/*
 *  USB Device Initialize Function
 *   Called by the User to initialize USB
 *    Return Value:    None
 */

/* Done 1st */
void USBD_Init(void)
{
    USB_CLK_CFG_t tClkCfg =
    {
        .eClk = USB_CLK_MCCR,
        .eMccr = USB_CLK_MCCR_PLL,
        .un8MccrDiv = 1,
    };

    USBD_HighSpeed = __TRUE;
		
    s_un8FAddr = 0;
    HAL_USB_Init(USB_ID_0);
    HAL_USB_SetClkConfig(USB_ID_0, &tClkCfg);
    HAL_USB_SetConfig(USB_ID_0, NULL);
    HAL_USB_SetIRQ(USB_ID_0, USB_OPS_INTR, USB_HW_IRQHandler, NULL, 0, false);
    USB_LOG("Init USB Done!\n");
}


/*
 *  USB Device Connect Function
 *   Called by the User to Connect/Disconnect USB Device
 *    Parameters:      con:   Connect/Disconnect
 *    Return Value:    None
 */

/* Done 1st */
void USBD_Connect(BOOL con)
{

}


/*
 *  USB Device Reset Function
 *   Called automatically on USB Device Reset
 *    Return Value:    None
 */

/* Done 1st */
void USBD_Reset(void)
{
    s_un8FAddr = 0;
    HAL_USB_SetConnect(USB_ID_0, true);
    s_bFaddr = false;
    USB_LOG("Reset USB!\n");
}


/*
 *  USB Device Suspend Function
 *   Called automatically on USB Device Suspend
 *    Return Value:    None
 */

/* no impl */
void USBD_Suspend(void)
{

}


/*
 *  USB Device Resume Function
 *   Called automatically on USB Device Resume
 *    Return Value:    None
 */

/* no impl */
void USBD_Resume(void)
{
    /* Performed by Hardware                                                    */
}


/*
 *  USB Device Remote Wakeup Function
 *   Called automatically on USB Device Remote Wakeup
 *    Return Value:    None
 */

/* no impl */
void USBD_WakeUp(void)
{

}


/*
 *  USB Device Remote Wakeup Configuration Function
 *    Parameters:      cfg:   Device Enable/Disable
 *    Return Value:    None
 */

/* no impl */
void USBD_WakeUpCfg(BOOL cfg)
{
    /* Not needed                                                               */
}


/*
 *  USB Device Set Address Function
 *    Parameters:      adr:   USB Device Address
 *                     setup: Called in setup stage (!=0), else after status stage
 *    Return Value:    None
 */

/* Done 1st */
void USBD_SetAddress(U32 adr, U32 setup)
{
    USB_LOG("SetAddr=0x%x\n",adr);
    s_un8FAddr = adr;
}


/*
 *  USB Device Configure Function
 *    Parameters:      cfg:   Device Configure/Deconfigure
 *    Return Value:    None
 */

/* Need? */
void USBD_Configure(BOOL cfg)
{
}


/*
 *  Configure USB Device Endpoint according to Descriptor
 *    Parameters:      pEPD:  Pointer to Device Endpoint Descriptor
 *    Return Value:    None
 */

/* Done 1st */
void USBD_ConfigEP(USB_ENDPOINT_DESCRIPTOR *pEPD)
{
    /* Double Buffering is not yet supported                                    */
    USB_EPX_CFG_t tEPxCfg;

    
    tEPxCfg.un8Num = pEPD->bEndpointAddress & 0x0F; 
    tEPxCfg.un16PktSize = pEPD->wMaxPacketSize;
    tEPxCfg.bDir = (pEPD->bEndpointAddress & 0x80 ? true : false); 
    tEPxCfg.un8Attr = pEPD->bmAttributes;

    USB_LOG("EPSetup:0x%x:0x%x:0x%x|0x%x|0x%x\n",tEPxCfg.un8Num, tEPxCfg.un16PktSize, tEPxCfg.bDir, tEPxCfg.un8Attr, pEPD->bInterval);
    HAL_USB_SetEPxConfig(USB_ID_0, &tEPxCfg);
}


/*
 *  Set Direction for USB Device Control Endpoint
 *    Parameters:      dir:   Out (dir == 0), In (dir <> 0)
 *    Return Value:    None
 */

void USBD_DirCtrlEP(U32 dir)
{
    if (ep0_setup_processing)
    {
        if (dir == REQUEST_HOST_TO_DEVICE)
        {
            ep0_h2d_candidate = 1;
        }
        else
        {
            ep0_h2d_candidate = 0;
            ep0_wait_data_out = 0;
        }
    }

    /* Not needed                                                               */
}


/*
 *  Enable USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

/* Need? */
void USBD_EnableEP(U32 EPNum)
{
}


/*
 *  Disable USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

/* Need? */
void USBD_DisableEP(U32 EPNum)
{
}


/*
 *  Reset USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

/* Need? */
void USBD_ResetEP(U32 EPNum)
{
}


/*
 *  Set Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

/* Stall? */
void USBD_SetStallEP(U32 EPNum)
{
    USB_LOG("SetStallEP\n");

    HAL_USB_SetEPxInControl(USB_ID_0, EPNum, USB_EP0_CTRL_SENDSTALL);

    if ((EPNum == 0x00U) || (EPNum == 0x80U))
    {
        ep0_setup_stalled = 1;
        ep0_h2d_candidate = 0;
        ep0_wait_data_out = 0;
    }
}


/*
 *  Clear Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */
/* Clear Stall? */
void USBD_ClrStallEP(U32 EPNum)
{
    USB_LOG("ClearStallEP\n");
    HAL_USB_SetEPxInControl(USB_ID_0, EPNum, USB_EP0_CTRL_SRVOUTPKTRDY | USB_EP0_CTRL_SRVSETUPEND);
}


/*
 *  Clear USB Device Endpoint Buffer
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ClearEPBuf(U32 EPNum)
{
}


/*
 *  Read USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *    Return Value:    Number of bytes read
 */

/* Done 1st */
U32 USBD_ReadEP(U32 EPNum, U8 *pData, U32 bufsz)
{
    uint8_t un8EpNum = (uint8_t)(EPNum & 0x0F);
    uint32_t un32FifoCnt = 0;

    HAL_USB_GetEPxFifoCount(USB_ID_0, un8EpNum, (uint16_t *)&un32FifoCnt);
    if (un32FifoCnt > bufsz)
    {
        un32FifoCnt = bufsz; 
    }

    HAL_USB_GetEPxFifo(USB_ID_0, un8EpNum, pData, (uint8_t)un32FifoCnt);

    if (un8EpNum == 0) 
    {
        HAL_USB_SetEPxInControl(USB_ID_0, un8EpNum, USB_EP0_CTRL_SRVOUTPKTRDY);
#if DEBUG
        if (un32FifoCnt == 8)
        {
            USB_LOG("0x%x|0x%x|0x%x\n", pData[0], pData[1], (pData[3] << 8 | pData[2]));
        }
#endif
    }
    else
    {
        HAL_USB_SetEPxOutControl(USB_ID_0, un8EpNum, USB_EPX_OUT_CTRL_OUTPKTRDY | USB_EPX_OUT_CTRL_FLUSHFIFO);
    }

    return (un32FifoCnt);
}


/*
 *  Write USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *                     cnt:   Number of bytes to write
 *    Return Value:    Number of bytes written
 */

extern USB_SETUP_PACKET USBD_SetupPacket;
extern USBD_EP_DATA USBD_EP0Data;

U32 USBD_WriteEP(U32 EPNum, U8 *pData, U32 cnt)
{
    uint8_t un8EpNum = EPNum & 0x0F;
	
    if (un8EpNum == 0)
    {
        if (cnt == 0)
        {
            if (ep0_setup_processing)
            {
                HAL_USB_SetEPxInControl(USB_ID_0, un8EpNum, USB_EP0_CTRL_SRVOUTPKTRDY | USB_EP0_CTRL_DATAEND);
            }
            else
            {
                HAL_USB_SetEPxInControl(USB_ID_0, un8EpNum, USB_EP0_CTRL_INPKTRDY | USB_EP0_CTRL_DATAEND);
            }
            ep0_status_in_zlp = 1;
            ep0_wait_data_out = 0;
            return (cnt);
        }
    }
    else
    {
        if (cnt == 0)
        {
            HAL_USB_SetEPxInControl(USB_ID_0, un8EpNum, USB_EPX_IN_CTRL_FLUSHFIFO);
            return (cnt);
        }
        
    }

    if (un8EpNum == 1)
        PB->BCR = 1 << 8;

    HAL_USB_SetEPxFifo(USB_ID_0, un8EpNum, pData, cnt);
#if DUMP
    for(int i = 0; i < cnt; i++)
        USB_LOG("0x%x,",pData[i]);
#endif
    if (un8EpNum == 0)
    {
        uint32_t un32Flag = USB_EP0_CTRL_INPKTRDY;
			
        USB_LOG("EP0Data.Count %d:cnt %d\n",USBD_EP0Data.Count, cnt);
        if (*(volatile uint32_t *)&USBD_EP0Data.Count == cnt)
        {
            un32Flag |= USB_EP0_CTRL_DATAEND;
        }

        HAL_USB_SetEPxInControl(USB_ID_0, un8EpNum, un32Flag);

        USB_LOG("EP0Data.Count %d:cnt %d:flag 0x%x\n",USBD_EP0Data.Count, cnt, un32Flag);
    }
    else
    {
        USB_LOG("EPx cnt=%d\n",cnt);
        HAL_USB_SetEPxInControl(USB_ID_0, un8EpNum, USB_EPX_IN_CTRL_INPKTRDY);
        USB_LOG("EPx WriteEnd!\n");
    }

    return (cnt);
}

/*
 *  Get USB Device Last Frame Number
 *    Parameters:      None
 *    Return Value:    Frame Number
 */

/* no impl */
U32 USBD_GetFrame(void)
{
    return 0;
}


#ifdef __RTX
U32 LastError;                          /* Last Error                         */

/*
 *  Get USB Last Error Code
 *    Parameters:      None
 *    Return Value:    Error Code
 */

U32 USBD_GetError(void)
{
    return (LastError);
}
#endif


/*
 *  USB Device Interrupt Service Routine
 */

static void USB_HW_IRQHandler(uint32_t un32Event, void *pContext)
{
    NVIC_DisableIRQ(USB_IRQn);
    USBD_SignalHandler();
}

void USBD_Handler(void)
{
    USB_EVENT_e eUsbEvt;
    USB_Type *ptUsb = (USB_Type *)USB_BASE;

    volatile uint32_t un32InEpEvent = 0, un32OutEpEvent = 0;
    volatile uint32_t un32CommonEvent = 0, un32Ep0Event = 0;

    un32CommonEvent = ptUsb->INTRUSB;
    un32OutEpEvent = ptUsb->INTROUT1;
    un32InEpEvent = ptUsb->INTRIN1;

    if (un32InEpEvent & 0x01)
    {
        ptUsb->INDEX = 0;
        un32Ep0Event = ptUsb->CSR0;

        if (un32Ep0Event == 0)
        {
            if (USBD_P_EP[0]) USBD_P_EP[0](USBD_EVT_IN);
        }

        if (s_un8FAddr != 0) 
        {
            HAL_USB_SetAddress(USB_ID_0, s_un8FAddr);
            s_un8FAddr = 0;
            USB_LOG("[S] Address Set\n");
        }

        if (un32Ep0Event & USB_EP0_EVENT_SENTSTALL)
        {
            HAL_USB_SetEPxInControl(USB_ID_0, 0, 0);
        }

        if (un32Ep0Event & (USB_EP0_EVENT_SETUPEND | USB_EP0_EVENT_SRVSETUPEND)) 
        {
            USB_LOG("Setup End!\n");
            HAL_USB_SetEPxInControl(USB_ID_0, 0, USB_EP0_CTRL_SRVSETUPEND);
            ep0_wait_data_out = 0;
        }

        if (un32Ep0Event & (USB_EP0_EVENT_OUTPKTRDY | USB_EP0_EVENT_SRVOUTPKTRDY)) 
        {

            if (ep0_wait_data_out)
            {
                ep0_wait_data_out = 0;

                if (USBD_P_EP[0])
                {
                    USBD_P_EP[0](USBD_EVT_OUT);
                }
            }
            else
            {

                ep0_setup_processing = 1;
                ep0_h2d_candidate    = 0;
                ep0_setup_stalled    = 0;
                ep0_status_in_zlp    = 0;

                if (USBD_P_EP[0])
                {
                    USBD_P_EP[0](USBD_EVT_SETUP);
                }

                ep0_setup_processing = 0;

                if ((ep0_h2d_candidate != 0U) &&
                    (ep0_setup_stalled == 0U) &&
                    (ep0_status_in_zlp == 0U))
                {
                    ep0_wait_data_out = 1;
                }
                else
                {
                    ep0_wait_data_out = 0;
                }
            }
        }

        if (un32Ep0Event & USB_EP0_EVENT_INPKTRDY) 
        {
            if (USBD_P_EP[0]) USBD_P_EP[0](USBD_EVT_IN);
        }
    }
    
    if (un32CommonEvent != 0) 
    {
        if (un32CommonEvent & USB_CM_EVENT_RESET) 
        {
            USBD_Reset();
            if (USBD_P_Reset_Event) USBD_P_Reset_Event();
        }

        if (un32CommonEvent & USB_CM_EVENT_SUSPEND) 
        {
            if (USBD_P_Suspend_Event) USBD_P_Suspend_Event();
        }

        if (un32CommonEvent & USB_CM_EVENT_RESUME) 
        {
            if (USBD_P_Resume_Event) USBD_P_Resume_Event();
        }

        if (un32CommonEvent & USB_CM_EVENT_SOF) 
        {
            if (USBD_P_SOF_Event) USBD_P_SOF_Event();
        }
    }

    if (un32OutEpEvent != 0)
    {
        uint8_t u32Num = 31 - __clz(un32OutEpEvent);
        if (USBD_P_EP[u32Num]) USBD_P_EP[u32Num](USBD_EVT_OUT);
    }

    if ((un32InEpEvent & 0x1E) != 0)
    {
        uint8_t u32Num = 31 - __clz(un32InEpEvent);
        
        ptUsb->INDEX = u32Num;

        uint32_t un32InCsr = ptUsb->INCSR1;
        if (USBD_P_EP[u32Num]) USBD_P_EP[u32Num](USBD_EVT_IN);
    }

    NVIC_EnableIRQ(USB_IRQn);
}

