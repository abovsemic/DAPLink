/**
 * @file    Driver_USART_a31g33x.c
 * @brief   CMSIS-Driver USART (receive only) for the SWO trace of A31G33x
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 *
 * Driver_USART0 = USART13, used by cmsis-dap/SWO.c (SWO_UART_DRIVER 0).
 *  - RX pin : PA1 (AF2 = RXD13), shared with the JTAG TDO (JTAG_TDO_PIN).
 *             AF2 only while the receiver is enabled (ARM_USART_CONTROL_RX = 1),
 *             GPIO input otherwise.
 *  - RX data: DMA (USART13 RX request), byte size, completion by the DMA interrupt
 *             (HPL DMA dispatcher, DMA_PERI_USART). No USART interrupt is used.
 *             The channel is configured once (HPL_DMA_SetConfig) and each block only
 *             writes MAR/TRANSCNT/CHnEN, so the next block starts with a short gap.
 *             Needs CONFIG_HAL_DMAC (hpl_dma.c).
 *  - TX, synchronous modes and modem lines are not supported.
 */

#include "DAP_config.h"
#include "DAP.h"

#if (SWO_UART != 0) && (SWO_UART_DRIVER == 0)

#include "abov_config.h"
#include "Driver_USART.h"
#include "hal_pcu.h"
#include "hal_usart.h"
#include "hpl_dma.h"

#if !defined(_DMAC)
#error "SWO UART driver uses the DMA: add CONFIG_HAL_DMAC"
#endif

#define ARM_USART_DRV_VERSION   ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)

#define SWO_USART_ID            USART_ID_3
#define SWO_USART               ((USART1n_Type *)USART13_BASE)
#define SWO_USART_BAUD_ERR_PCT  2U                  /* max baudrate error in % */

#define SWO_RX_PORT             ((PCU_ID_e)JTAG_TDO_PIN_PORT)   /* PA1 */
#define SWO_RX_PIN              ((PCU_PIN_ID_e)JTAG_TDO_PIN)
#define SWO_RX_ALT              PCU_ALT_2           /* PA1: RXD13 */

#define SWO_DMA_CH              DMA_ID_0
#define SWO_DMA                 ((DMACn_Type *)(DMAC0_BASE + (0x20U * (uint32_t)SWO_DMA_CH)))
#define SWO_DMA_MAX_LEN         0xFFFU              /* DMACn_CR.TRANSCNT is 12 bits */

#define SWO_USART_ERR_Msk       (USART1n_ST_DORn_Msk | USART1n_ST_FEn_Msk | USART1n_ST_PEn_Msk)

static struct {
    ARM_USART_SignalEvent_t cb_event;
    uint8_t                 initialized;
    uint8_t                 powered;
    uint8_t                 configured;
    uint8_t                 rx_enabled;
    volatile uint8_t        rx_busy;
    uint8_t                 rx_overflow;
    uint8_t                 rx_framing_error;
    uint8_t                 rx_parity_error;
    uint32_t                rx_num;
    uint32_t                rx_start;           /* buffer address of the running block */
    volatile uint32_t       rx_cnt;             /* valid when rx_busy == 0 */
} swo_usart;

static const ARM_DRIVER_VERSION DriverVersion = {
    ARM_USART_API_VERSION,
    ARM_USART_DRV_VERSION
};

static const ARM_USART_CAPABILITIES DriverCapabilities = {
    1,  /* supports UART (Asynchronous) mode */
    0,  /* supports Synchronous Master mode */
    0,  /* supports Synchronous Slave mode */
    0,  /* supports UART Single-wire mode */
    0,  /* supports UART IrDA mode */
    0,  /* supports UART Smart Card mode */
    0,  /* Smart Card Clock generator available */
    0,  /* RTS Flow Control available */
    0,  /* CTS Flow Control available */
    0,  /* Transmit completed event: \ref ARM_USART_EVENT_TX_COMPLETE */
    0,  /* Signal receive character timeout event: \ref ARM_USART_EVENT_RX_TIMEOUT */
    0,  /* RTS Line: 0=not available, 1=available */
    0,  /* CTS Line: 0=not available, 1=available */
    0,  /* DTR Line: 0=not available, 1=available */
    0,  /* DSR Line: 0=not available, 1=available */
    0,  /* DCD Line: 0=not available, 1=available */
    0,  /* RI Line: 0=not available, 1=available */
    0,  /* Signal CTS change event: \ref ARM_USART_EVENT_CTS */
    0,  /* Signal DSR change event: \ref ARM_USART_EVENT_DSR */
    0,  /* Signal DCD change event: \ref ARM_USART_EVENT_DCD */
    0,  /* Signal RI change event: \ref ARM_USART_EVENT_RI */
    0   /* Reserved */
};

/* Baudrate generator: BDR = PCLK / (OVS * baud) - 1 with OVS = 16 or 8 (double speed),
 * no fractional divider on this USART. Returns the exact baudrate to program (0 = not
 * possible within SWO_USART_BAUD_ERR_PCT) and selects the oversampling. */
static uint32_t SWO_USART_GetBaud(uint32_t baud, bool *pbDoubleSpeed)
{
    static const uint32_t ovs_tbl[2] = { 16U, 8U };
    uint32_t pclk = SystemPeriClock;
    uint32_t best = 0U, best_err = 0xFFFFFFFFU;
    uint32_t i, ovs, n, actual, err;

    for (i = 0U; i < 2U; i++) {
        ovs = ovs_tbl[i];
        n = (pclk + ((ovs * baud) / 2U)) / (ovs * baud);
        if ((n == 0U) || (n > 0x10000U)) {
            continue;
        }
        actual = pclk / (ovs * n);
        err = (actual > baud) ? (actual - baud) : (baud - actual);
        if (err < best_err) {
            best_err = err;
            best = actual;
            *pbDoubleSpeed = (ovs == 8U);
        }
    }
    if ((best == 0U) || ((best_err * 100U) > (baud * SWO_USART_BAUD_ERR_PCT))) {
        return 0U;
    }
    return best;
}

/* Line errors latched by the USART since the last call, as ARM_USART_EVENT_RX_xxx */
static uint32_t SWO_USART_GetErrorEvents(void)
{
    uint32_t st = SWO_USART->ST & SWO_USART_ERR_Msk;
    uint32_t event = 0U;

    if (st != 0U) {
        SWO_USART->ST = st;
        if (st & USART1n_ST_DORn_Msk) {
            swo_usart.rx_overflow = 1U;
            event |= ARM_USART_EVENT_RX_OVERFLOW;
        }
        if (st & USART1n_ST_FEn_Msk) {
            swo_usart.rx_framing_error = 1U;
            event |= ARM_USART_EVENT_RX_FRAMING_ERROR;
        }
        if (st & USART1n_ST_PEn_Msk) {
            swo_usart.rx_parity_error = 1U;
            event |= ARM_USART_EVENT_RX_PARITY_ERROR;
        }
    }
    return event;
}

/* Configure the DMA channel once: USART13 RX request, byte, peripheral to memory */
static int32_t SWO_USART_SetupDma(void)
{
    DMA_CFG_t tDmaCfg = {
        .ePeri       = DMA_PERI_USART,
        .ePeriId     = (DMA_PERI_ID_e)SWO_USART_ID,
        .un32PeriNum = 0U,
        .eSize       = DMA_BUS_SIZE_BYTE,
        .eDir        = DMA_DIR_PERI_TO_MEM
    };

    if (HPL_DMA_GetPeriSelectNumber(DMA_PERI_USART, (DMA_PERI_ID_e)SWO_USART_ID,
                                    DMA_PERI_COM_RX, &tDmaCfg.un32PeriNum) != HAL_ERR_OK) {
        return ARM_DRIVER_ERROR;
    }
    /* Also registers the channel for the HPL IRQ dispatcher (DMA_PERI_USART) */
    if (HPL_DMA_SetConfig(SWO_DMA_CH, &tDmaCfg) != HAL_ERR_OK) {
        return ARM_DRIVER_ERROR;
    }
    SWO_DMA->CR  &= ~DMACn_CR_CHnEN_Msk;
    SWO_DMA->PAR  = (SWO_DMA->PAR & ~DMACn_PAR_POADR_Msk) |
                    ((uint32_t)&SWO_USART->RDR & DMACn_PAR_POADR_Msk);
    SWO_DMA->IESR = DMACn_IESR_TRCIFGn_Msk | DMACn_IESR_TRERIFGn_Msk;  /* clear, irq off */
    return ARM_DRIVER_OK;
}

static void SWO_USART_StopDma(void)
{
    SWO_DMA->CR  &= ~DMACn_CR_CHnEN_Msk;
    SWO_DMA->IESR = DMACn_IESR_TRCIFGn_Msk | DMACn_IESR_TRERIFGn_Msk;  /* clear, irq off */
}

/* Start one block: only MAR, TRANSCNT and the enables (short gap between blocks) */
static void SWO_USART_StartDma(uint8_t *buf, uint32_t num)
{
    SWO_DMA->MAR  = (uint32_t)buf;
    SWO_DMA->CR   = (SWO_DMA->CR & ~DMACn_CR_TRANSCNT_Msk) |
                    ((num << DMACn_CR_TRANSCNT_Pos) & DMACn_CR_TRANSCNT_Msk);
    /* Only the transfer complete interrupt: the HPL dispatcher does not clear the error flag */
    SWO_DMA->IESR = DMACn_IESR_TRCIENn_Msk;
    SWO_DMA->CR  |= DMACn_CR_CHnEN_Msk;
}

/* DMA transfer complete (HPL DMA interrupt, priority 0) */
static void SWO_USART_DmaHandler(uint32_t un32Event, void *pContext)
{
    uint32_t event;

    (void)un32Event;
    (void)pContext;

    if (swo_usart.rx_busy == 0U) {
        return;
    }

    SWO_USART_StopDma();
    swo_usart.rx_cnt  = swo_usart.rx_num;
    swo_usart.rx_busy = 0U;

    event = ARM_USART_EVENT_RECEIVE_COMPLETE | SWO_USART_GetErrorEvents();
    if (swo_usart.cb_event != NULL) {
        swo_usart.cb_event(event);              /* may call Receive() for the next block */
    }
}

static void SWO_USART_SetRxPin(bool usart)
{
    if (usart) {
        (void)HAL_PCU_SetPullUpDown(SWO_RX_PORT, SWO_RX_PIN, PCU_PUPD_UP);
        (void)HAL_PCU_SetAltMode(SWO_RX_PORT, SWO_RX_PIN, SWO_RX_ALT);
    } else {
        (void)HAL_PCU_SetInOutMode(SWO_RX_PORT, SWO_RX_PIN, PCU_INOUT_INPUT);
        (void)HAL_PCU_SetPullUpDown(SWO_RX_PORT, SWO_RX_PIN, PCU_PUPD_DISABLED);
    }
}

static void SWO_USART_AbortReceive(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (swo_usart.rx_busy != 0U) {
        SWO_USART_StopDma();
        swo_usart.rx_busy = 0U;
    }
    swo_usart.rx_cnt = 0U;
    __set_PRIMASK(primask);
}

//**************************************************************************************************
// CMSIS-Driver USART functions

static ARM_DRIVER_VERSION ARM_USART_GetVersion(void)
{
    return DriverVersion;
}

static ARM_USART_CAPABILITIES ARM_USART_GetCapabilities(void)
{
    return DriverCapabilities;
}

static int32_t ARM_USART_Initialize(ARM_USART_SignalEvent_t cb_event)
{
    swo_usart.cb_event = cb_event;

    if (swo_usart.initialized == 0U) {
        if (HPL_DMA_Init(DMA_PERI_USART, (uint8_t)SWO_USART_ID) != HAL_ERR_OK) {
            return ARM_DRIVER_ERROR;
        }
        if (HPL_DMA_SetIRQ(DMA_PERI_USART, SWO_USART_DmaHandler, NULL, true) != HAL_ERR_OK) {
            return ARM_DRIVER_ERROR;
        }
        if (SWO_USART_SetupDma() != ARM_DRIVER_OK) {
            return ARM_DRIVER_ERROR;
        }
        swo_usart.initialized = 1U;
    }
    return ARM_DRIVER_OK;
}

static int32_t ARM_USART_Uninitialize(void)
{
    SWO_USART_AbortReceive();
    (void)HPL_DMA_Stop(SWO_DMA_CH);                 /* releases the channel */
    (void)HPL_DMA_SetIRQ(DMA_PERI_USART, NULL, NULL, false);
    (void)HPL_DMA_Uninit(DMA_PERI_USART, (uint8_t)SWO_USART_ID);
    swo_usart.cb_event    = NULL;
    swo_usart.initialized = 0U;
    return ARM_DRIVER_OK;
}

static int32_t ARM_USART_PowerControl(ARM_POWER_STATE state)
{
    switch (state) {
        case ARM_POWER_OFF:
            SWO_USART_AbortReceive();
            if (swo_usart.powered != 0U) {
                SWO_USART->CR1 &= ~USART1n_CR1_RXEn_Msk;
                SWO_USART_SetRxPin(false);
                (void)HAL_USART_Uninit(SWO_USART_ID);
            }
            swo_usart.powered    = 0U;
            swo_usart.configured = 0U;
            swo_usart.rx_enabled = 0U;
            return ARM_DRIVER_OK;

        case ARM_POWER_FULL:
            if (swo_usart.initialized == 0U) {
                return ARM_DRIVER_ERROR;
            }
            if (swo_usart.powered == 0U) {
                if (HAL_USART_Init(SWO_USART_ID) != HAL_ERR_OK) {
                    return ARM_DRIVER_ERROR;
                }
                swo_usart.powered = 1U;
            }
            swo_usart.rx_overflow      = 0U;
            swo_usart.rx_framing_error = 0U;
            swo_usart.rx_parity_error  = 0U;
            return ARM_DRIVER_OK;

        case ARM_POWER_LOW:
        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
}

static int32_t ARM_USART_Send(const void *data, uint32_t num)
{
    (void)data;
    (void)num;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t ARM_USART_Receive(void *data, uint32_t num)
{
    if ((data == NULL) || (num == 0U) || (num > SWO_DMA_MAX_LEN)) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    if (swo_usart.powered == 0U) {
        return ARM_DRIVER_ERROR;
    }
    if (swo_usart.rx_busy != 0U) {
        return ARM_DRIVER_ERROR_BUSY;
    }

    swo_usart.rx_num   = num;
    swo_usart.rx_start = (uint32_t)data;
    swo_usart.rx_cnt   = 0U;
    swo_usart.rx_busy  = 1U;
    SWO_USART_StartDma((uint8_t *)data, num);

    return ARM_DRIVER_OK;
}

static int32_t ARM_USART_Transfer(const void *data_out, void *data_in, uint32_t num)
{
    (void)data_out;
    (void)data_in;
    (void)num;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static uint32_t ARM_USART_GetTxCount(void)
{
    return 0U;
}

static uint32_t ARM_USART_GetRxCount(void)
{
    uint32_t primask;
    uint32_t count;
    uint32_t by_cnt;

    primask = __get_PRIMASK();
    __disable_irq();
    if (swo_usart.rx_busy != 0U) {
        /* Bytes done from TRANSCNT (counts down) and from MAR (counts up): the smaller one,
         * so a register that is not updated by the DMA never reports unreceived bytes. */
        by_cnt = swo_usart.rx_num -
                 ((SWO_DMA->CR & DMACn_CR_TRANSCNT_Msk) >> DMACn_CR_TRANSCNT_Pos);
        count  = SWO_DMA->MAR - swo_usart.rx_start;
        if (by_cnt < count) {
            count = by_cnt;
        }
        if (count > swo_usart.rx_num) {
            count = 0U;
        }
    } else {
        count = swo_usart.rx_cnt;
    }
    __set_PRIMASK(primask);

    return count;
}

static int32_t ARM_USART_Control(uint32_t control, uint32_t arg)
{
    USART_CFG_t tCfg = { (USART_MODE_e)0 };
    uint32_t baud;
    bool bDoubleSpeed = false;

    if (swo_usart.powered == 0U) {
        return ARM_DRIVER_ERROR;
    }

    switch (control & ARM_USART_CONTROL_Msk) {
        case ARM_USART_MODE_ASYNCHRONOUS:
            break;

        case ARM_USART_CONTROL_RX:
            if (arg != 0U) {
                if (swo_usart.configured == 0U) {
                    return ARM_DRIVER_ERROR;
                }
                (void)SWO_USART_GetErrorEvents();
                swo_usart.rx_overflow      = 0U;
                swo_usart.rx_framing_error = 0U;
                swo_usart.rx_parity_error  = 0U;
                SWO_USART_SetRxPin(true);
                SWO_USART->CR1 |= USART1n_CR1_RXEn_Msk;
                swo_usart.rx_enabled = 1U;
            } else {
                SWO_USART->CR1 &= ~USART1n_CR1_RXEn_Msk;
                SWO_USART_SetRxPin(false);
                swo_usart.rx_enabled = 0U;
            }
            return ARM_DRIVER_OK;

        case ARM_USART_ABORT_RECEIVE:
            SWO_USART_AbortReceive();
            return ARM_DRIVER_OK;

        case ARM_USART_CONTROL_TX:
            return (arg == 0U) ? ARM_DRIVER_OK : ARM_DRIVER_ERROR_UNSUPPORTED;

        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }

    /* ARM_USART_MODE_ASYNCHRONOUS: only 8N1 without flow control */
    if ((control & ARM_USART_DATA_BITS_Msk) != ARM_USART_DATA_BITS_8) {
        return ARM_USART_ERROR_DATA_BITS;
    }
    if ((control & ARM_USART_PARITY_Msk) != ARM_USART_PARITY_NONE) {
        return ARM_USART_ERROR_PARITY;
    }
    if ((control & ARM_USART_STOP_BITS_Msk) != ARM_USART_STOP_BITS_1) {
        return ARM_USART_ERROR_STOP_BITS;
    }
    if ((control & ARM_USART_FLOW_CONTROL_Msk) != ARM_USART_FLOW_CONTROL_NONE) {
        return ARM_USART_ERROR_FLOW_CONTROL;
    }
    if ((arg == 0U) || (arg > SWO_UART_MAX_BAUDRATE)) {
        return ARM_USART_ERROR_BAUDRATE;
    }
    baud = SWO_USART_GetBaud(arg, &bDoubleSpeed);
    if (baud == 0U) {
        return ARM_USART_ERROR_BAUDRATE;
    }

    tCfg.eMode                   = USART_MODE_UART;
    tCfg.un32BaudRate            = baud;
    tCfg.tCfg.tUart.eData        = USART_DATA_8;
    tCfg.tCfg.tUart.eParity      = USART_PARITY_NONE;
    tCfg.tCfg.tUart.eStop        = USART_STOP_1;
    tCfg.tCfg.tUart.bDoubleSpeed = bDoubleSpeed;

    if (HAL_USART_SetConfig(SWO_USART_ID, &tCfg) != HAL_ERR_OK) {
        return ARM_USART_ERROR_BAUDRATE;
    }
    /* SetConfig enables TX and RX: the receiver is controlled by ARM_USART_CONTROL_RX */
    if (swo_usart.rx_enabled == 0U) {
        SWO_USART->CR1 &= ~USART1n_CR1_RXEn_Msk;
    }
    swo_usart.configured = 1U;

    return ARM_DRIVER_OK;
}

static ARM_USART_STATUS ARM_USART_GetStatus(void)
{
    ARM_USART_STATUS status = { 0 };

    status.rx_busy          = swo_usart.rx_busy;
    status.rx_overflow      = swo_usart.rx_overflow;
    status.rx_framing_error = swo_usart.rx_framing_error;
    status.rx_parity_error  = swo_usart.rx_parity_error;

    return status;
}

static int32_t ARM_USART_SetModemControl(ARM_USART_MODEM_CONTROL control)
{
    (void)control;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static ARM_USART_MODEM_STATUS ARM_USART_GetModemStatus(void)
{
    ARM_USART_MODEM_STATUS status = { 0 };

    return status;
}

ARM_DRIVER_USART Driver_USART0 = {
    ARM_USART_GetVersion,
    ARM_USART_GetCapabilities,
    ARM_USART_Initialize,
    ARM_USART_Uninitialize,
    ARM_USART_PowerControl,
    ARM_USART_Send,
    ARM_USART_Receive,
    ARM_USART_Transfer,
    ARM_USART_GetTxCount,
    ARM_USART_GetRxCount,
    ARM_USART_Control,
    ARM_USART_GetStatus,
    ARM_USART_SetModemControl,
    ARM_USART_GetModemStatus
};

#endif /* (SWO_UART != 0) && (SWO_UART_DRIVER == 0) */
