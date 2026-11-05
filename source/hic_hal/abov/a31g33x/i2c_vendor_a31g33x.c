/**
 * @file    i2c_vendor_a31g33x.c
 * @brief   I2C master bridge over CMSIS-DAP vendor commands for A31G33x (interrupt driven)
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 *
 * I2C2 master on SCL2 = PA8, SDA2 = PA9 (AF5, open-drain, pull-ups on the target board).
 * Touch IC interrupt input on PA10 (EINT0), used as stream trigger.
 * The commands use the DAP vendor extended IDs and come from two paths:
 *  - the CMSIS-DAP v2 interface (DAP_ProcessVendorCommandEx, __WEAK in DAP.c), packet DAP_PACKET_SIZE
 *  - the I2C USB interface (own WinUSB interface and EP5, one command per transfer), packet
 *    I2CU_PACKET_SIZE: usable while a debugger (Keil, OpenOCD) holds the CMSIS-DAP interface.
 * Multi-byte fields are little-endian, len/count fields are 16 bit.
 *
 *  0xA0 I2C_Config     req: [A0][clock:4]                          rsp: [A0][status][clock:4]
 *                      clock = SCL in Hz (100000..400000), 0 = I2C off (pins back to GPIO input).
 *                      The response returns the programmed SCL (never above the request).
 *  0xA1 I2C_Write      req: [A1][addr][flags][len:2][data...]       rsp: [A1][status][count:2]
 *                      len = 0 sends only the address (device probe).
 *  0xA2 I2C_Read       req: [A2][addr][flags][len:2]                rsp: [A2][status][count:2][data...]
 *  0xA3 I2C_WriteRead  req: [A3][addr][flags][wlen:2][rlen:2][wdata...]
 *                                                                   rsp: [A3][status][count:2][rdata...]
 *                      Write, repeated START (or STOP + START with I2C_FLAG_STOP_START), read.
 *  0xA4 I2C_Scan       req: [A4]                                    rsp: [A4][status][map:16]
 *                      Bit n of the map = address n ACKed (0x08..0x77 probed).
 *  0xA5 I2C_IntState   req: [A5]                                    rsp: [A5][status][level]
 *                      level = touch INT pin (PA10) input level.
 *  0xA6 StreamStart    req: [A6][addr][flags][mode][period:2][wlen][rlen][wdata...]
 *                                                                   rsp: [A6][status][tsclock:4]
 *                      Repeats the WriteRead [wdata] -> [rlen bytes] in the background:
 *                      mode 0 = every 'period' ms (1..1000), 1 = INT falling edge,
 *                      2 = INT rising edge, 3 = INT both edges. wlen 0..4, rlen 1..64.
 *                      tsclock = record timestamp clock in Hz.
 *  0xA7 StreamRead     req: [A7]                  rsp: [A7][status][overrun:2][missed:2][n][records...]
 *                      record = [status][count][timestamp:4][data:count], oldest first.
 *                      TIMEOUT record data (debug): [state][ST][CR][widx][ridx][SCL][SDA].
 *                      overrun = records dropped (buffer full), missed = triggers merged while
 *                      a transfer was running, both since the last StreamRead.
 *  0xA8 StreamStop     req: [A8]                                    rsp: [A8][status]
 *
 *  addr   : 7-bit slave address (8-bit write address >> 1)
 *  flags  : bit0 = no STOP, the bus is kept for a repeated START by the next Write/Read/WriteRead
 *           bit1 = WriteRead/stream: STOP + START between the write and the read phase
 *  count  : Write = data bytes ACKed by the slave, Read/WriteRead = data bytes received
 *  status : I2C_STATUS_xxx
 *
 * Transfers run in the I2C2 interrupt (state machine below). The DAP command thread starts a
 * transfer and sleeps on a thread flag until the interrupt reports the end. A transfer that does
 * not end in time (SCL held low) is aborted and the bus is recovered (I2C reset, 9 SCL pulses, STOP).
 * While a stream runs, the transfers are started by the trigger (INT edge or TIMER11) and the
 * results are queued in a ring buffer; the other I2C commands return I2C_STATUS_STREAMING.
 * A stuck stream transfer is detected (and the bus recovered) by the next StreamRead.
 *
 * Read/WriteRead responses can use the whole packet: do not put them in DAP_ExecuteCommands
 * behind other commands (the response would not fit).
 *
 * SCL = PCLK / ((4 x SCLR + 3) + (4 x SCHR + 3)).
 * The real SCL can be a bit slower than the response value (rise time, clock stretching).
 */

#include <string.h>
#include "cmsis_os2.h"
#include "compiler.h"
#include "DAP_config.h"
#include "DAP.h"
#include "hal_pcu.h"
#include "hal_timer1.h"
#if defined(BULK_ENDPOINT)
#include "rl_usb.h"
#include "usb_for_lib.h"
#endif

#if defined(_I2C)
#error "The I2C HAL (hal_i2c.c) also defines the I2C2 interrupt handler: remove CONFIG_HAL_I2C"
#endif
#if !defined(OS_TICK_FREQ)
#error "OS_TICK_FREQ (kernel tick in Hz) is needed for the transfer timeout"
#endif

// Vendor command IDs
#define ID_DAP_I2C_Config       0xA0U
#define ID_DAP_I2C_Write        0xA1U
#define ID_DAP_I2C_Read         0xA2U
#define ID_DAP_I2C_WriteRead    0xA3U
#define ID_DAP_I2C_Scan         0xA4U
#define ID_DAP_I2C_IntState     0xA5U
#define ID_DAP_I2C_StreamStart  0xA6U
#define ID_DAP_I2C_StreamRead   0xA7U
#define ID_DAP_I2C_StreamStop   0xA8U

// Response status
#define I2C_STATUS_OK           0x00U
#define I2C_STATUS_NACK_ADDR    0x01U   ///< address not acknowledged
#define I2C_STATUS_NACK_DATA    0x02U   ///< data byte not acknowledged (count = bytes ACKed)
#define I2C_STATUS_ARB_LOST     0x03U   ///< arbitration lost (other master)
#define I2C_STATUS_BUS_BUSY     0x04U   ///< bus still busy after a recovery
#define I2C_STATUS_TIMEOUT      0x05U   ///< transfer not finished in time (bus recovered)
#define I2C_STATUS_NOT_CONFIG   0x06U   ///< I2C_Config not done
#define I2C_STATUS_STREAMING    0x07U   ///< stream running: StreamStop first
#define I2C_STATUS_PARAM        0xFFU   ///< invalid request

// Request flags
#define I2C_FLAG_NO_STOP        0x01U
#define I2C_FLAG_STOP_START     0x02U

// Stream trigger modes
#define STREAM_MODE_PERIODIC    0U
#define STREAM_MODE_INT_FALL    1U
#define STREAM_MODE_INT_RISE    2U
#define STREAM_MODE_INT_BOTH    3U

// Data limits (request/response must fit in one packet of 'size' bytes: DAP packet or I2C interface buffer)
#define I2C_WRITE_MAX(size)     ((size) - 5U)
#define I2C_READ_MAX(size)      ((size) - 4U)
#define I2C_WR_WRITE_MAX(size)  ((size) - 7U)

#define STREAM_WLEN_MAX         4U
#define STREAM_RLEN_MAX         64U
#define STREAM_PERIOD_MAX       1000U               /* ms */
#define STREAM_REC_HDR          6U                  /* [status][count][timestamp:4] */
#define STREAM_RSP_HDR          6U                  /* [status][overrun:2][missed:2][n] */
#define STREAM_BUF_SIZE         512U                /* power of 2 (RAM is short: ~10 records of 43 bytes) */

// Hardware
#define I2CB                    ((I2Cn_Type *)I2C2_BASE)
#define I2CB_IRQn               I2C2_IRQn
#define I2CB_IRQ_PRIO           3U                  /* lowest, like the CDC UART: a late event only stretches SCL */
#define I2CB_PORT               PCU_ID_A
#define I2CB_SCL_PIN            PCU_PIN_ID_8        /* PA8: SCL2 */
#define I2CB_SDA_PIN            PCU_PIN_ID_9        /* PA9: SDA2 */
#define I2CB_ALT                PCU_ALT_5

#define I2CB_CLOCK_MIN          100000U             /* Standard-mode */
#define I2CB_CLOCK_MAX          400000U             /* Fast-mode */
#define I2CB_SCL_REG_MAX        0xFFFU              /* SCLR/SCHR are 12 bit */
#define I2CB_EVT_Msk            (I2Cn_ST_GCALLn_Msk | I2Cn_ST_TENDn_Msk | I2Cn_ST_STOPDn_Msk | \
                                 I2Cn_ST_SSELn_Msk | I2Cn_ST_MLOSTn_Msk)
#define I2CB_ST_CLEAR           0xFFU               /* clears the event bits, releases SCL */

#define I2CB_BUSY_TICKS         (TIMESTAMP_CLOCK / 500U)            /* bus free wait before a recovery: 2 ms */
#define I2CB_TBUF_TICKS         (TIMESTAMP_CLOCK / 200000U)         /* bus free time STOP -> START: 5 us */
#define I2CB_MARGIN_TICKS       (TIMESTAMP_CLOCK / 40U)             /* transfer timeout = 2 x bus time + 25 ms */
#define STREAM_MARGIN_TICKS     (TIMESTAMP_CLOCK / 40U)             /* stream: + 25 ms */
#define STREAM_DIAG_LEN         7U                  /* TIMEOUT record: [state][ST][CR][widx][ridx][SCL][SDA] */
#define I2CB_THREAD_FLAG        (1UL << 14)         /* RTX4 events are 16 bit, main_task uses bits 0..11 */
#define I2CB_OS_TICK_TS         (TIMESTAMP_CLOCK / OS_TICK_FREQ)    /* TIMESTAMP ticks per kernel tick */

#define TINT_PORT               TOUCH_INT_PIN_PORT
#define TINT_PIN                TOUCH_INT_PIN       /* PA10 */
#define TINT_EINT               0U                  /* EINT0: Interrupt3_Handler in hal_pcu.c */
#define TINT_IRQn               EINT0_IRQn
#define TINT_IRQ_PRIO           I2CB_IRQ_PRIO       /* same as I2C2: the two never preempt each other */

#define STIM_ID                 TIMER1_ID_1         /* TIMER11: periodic trigger (TIMER10 = sdk.c tick) */
#define STIM_PRESCALE           3999U               /* PREDR is 12 bit: PCLK 48 MHz / 4000 = 12 kHz */
#define STIM_COUNTS_PER_MS      12U                 /* 1000 ms = 12000 counts, fits the 16-bit ADR */
#define STIM_IRQ_PRIO           I2CB_IRQ_PRIO

#define I2CU_PACKET_SIZE        128U                /* I2C USB interface request/response buffer (RAM is short) */
#define I2CU_FS_PACKET          64U                 /* full-speed bulk packet */

COMPILER_ASSERT(I2C2_IRQn == 25);                   /* handler below = Interrupt25_Handler */
COMPILER_ASSERT((STREAM_BUF_SIZE & (STREAM_BUF_SIZE - 1U)) == 0U);
COMPILER_ASSERT((STREAM_RSP_HDR + STREAM_REC_HDR + STREAM_RLEN_MAX) < DAP_PACKET_SIZE);
#if defined(BULK_ENDPOINT)
COMPILER_ASSERT((STREAM_RSP_HDR + STREAM_REC_HDR + STREAM_RLEN_MAX) < I2CU_PACKET_SIZE);
COMPILER_ASSERT((I2CU_PACKET_SIZE % I2CU_FS_PACKET) == 0U);
#endif

typedef enum {
    I2CB_IDLE,
    I2CB_ADDR_W,                ///< SLA+W sent
    I2CB_WRITE,                 ///< data byte sent
    I2CB_ADDR_R,                ///< SLA+R sent
    I2CB_READ,                  ///< data byte received
    I2CB_STOP,                  ///< STOP requested, waiting for the STOP event
    I2CB_STOP_RD,               ///< STOP after the write phase, the read starts on the STOP event
} I2CB_State_e;

/* Transfer shared with the interrupt */
static struct {
    volatile uint8_t    state;
    volatile uint8_t    status;
    volatile uint8_t    done;
    uint8_t             addr;
    uint8_t             flags;
    uint8_t             wphase;         ///< transfer starts with SLA+W
    const uint8_t      *wbuf;
    uint32_t            wlen;
    volatile uint32_t   widx;           ///< bytes loaded into DR
    volatile uint32_t   wcnt;           ///< bytes ACKed
    uint8_t            *rbuf;
    uint32_t            rlen;
    volatile uint32_t   ridx;           ///< bytes received
    volatile uint32_t   t0;             ///< START time (TIMESTAMP_GET)
    osThreadId_t        waiter;         ///< DAP command thread, NULL = stream transfer
} s_xfer;

static uint32_t s_un32Clock;            ///< programmed SCL in Hz, 0 = I2C off
static volatile uint8_t s_bHeld;        ///< last transfer ended without STOP: SCL held low, still master

/* Background stream */
static struct {
    volatile uint8_t    active;
    volatile uint8_t    pending;        ///< trigger during a transfer: start again at the end
    uint8_t             mode;
    uint8_t             addr;
    uint8_t             flags;
    uint8_t             wlen;
    uint8_t             rlen;
    uint8_t             wbuf[STREAM_WLEN_MAX];
    uint8_t             rbuf[STREAM_RLEN_MAX];
    volatile uint32_t   ts;             ///< trigger time of the running transfer
    volatile uint32_t   pend_ts;        ///< trigger time of the pending transfer
    volatile uint16_t   overrun;
    volatile uint16_t   missed;
    volatile uint32_t   head;           ///< write index (free running), producers run with IRQs masked
    volatile uint32_t   tail;           ///< read index (free running), DAP command thread only
    uint8_t             buf[STREAM_BUF_SIZE];
} s_stream;

static PCU_Context_t s_tTintCtx;
static uint8_t s_bTintInit;

//**************************************************************************************************
// Helpers

static uint16_t I2CB_Get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static void I2CB_Put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void I2CB_Put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t I2CB_Min(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static void I2CB_Delay(uint32_t ticks)
{
    uint32_t t0 = TIMESTAMP_GET();

    while ((TIMESTAMP_GET() - t0) < ticks) {
    }
}

/* CR read-modify-write. I2CnIFLAG (status bit) is written as 0, never written back. */
static void I2CB_CrUpdate(uint32_t set, uint32_t clr)
{
    I2CB->CR = (I2CB->CR & ~(clr | I2Cn_CR_I2CnIFLAG_Msk)) | set;
}

/* SCLR + SCHR for a requested SCL: rounded up, so the SCL is never faster than requested.
 * Returns the sum (0 = out of range). */
static uint32_t I2CB_CalcPeriod(uint32_t hz)
{
    uint32_t total, n;

    if ((hz < I2CB_CLOCK_MIN) || (hz > I2CB_CLOCK_MAX)) {
        return 0U;
    }
    total = (SystemPeriClock + hz - 1U) / hz;               /* PCLK cycles per SCL period */
    if (total <= 6U) {
        return 0U;
    }
    n = (total - 6U + 3U) / 4U;                             /* 4 x (SCLR + SCHR) + 6 >= total */
    if (n < 2U) {
        n = 2U;
    }
    if (n > (2U * I2CB_SCL_REG_MAX)) {
        return 0U;                                          /* below the slowest SCL */
    }
    return n;
}

static uint32_t I2CB_ActualClock(void)
{
    uint32_t n = I2CB_CalcPeriod(s_un32Clock);

    return (n != 0U) ? (SystemPeriClock / (4U * n + 6U)) : 0U;
}

static void I2CB_SetPins(bool i2c)
{
    if (i2c) {
        (void)HAL_PCU_SetPullUpDown(I2CB_PORT, I2CB_SCL_PIN, PCU_PUPD_UP);
        (void)HAL_PCU_SetPullUpDown(I2CB_PORT, I2CB_SDA_PIN, PCU_PUPD_UP);
        (void)HAL_PCU_SetAltMode(I2CB_PORT, I2CB_SCL_PIN, I2CB_ALT);   /* open-drain in AF mode */
        (void)HAL_PCU_SetAltMode(I2CB_PORT, I2CB_SDA_PIN, I2CB_ALT);
    } else {
        (void)HAL_PCU_SetInOutMode(I2CB_PORT, I2CB_SCL_PIN, PCU_INOUT_INPUT);
        (void)HAL_PCU_SetInOutMode(I2CB_PORT, I2CB_SDA_PIN, PCU_INOUT_INPUT);
        (void)HAL_PCU_SetPullUpDown(I2CB_PORT, I2CB_SCL_PIN, PCU_PUPD_DISABLED);
        (void)HAL_PCU_SetPullUpDown(I2CB_PORT, I2CB_SDA_PIN, PCU_PUPD_DISABLED);
    }
}

static void I2CB_IrqEnable(bool enable)
{
    if (enable) {
        NVIC_ClearPendingIRQ(I2CB_IRQn);
        NVIC_SetPriority(I2CB_IRQn, I2CB_IRQ_PRIO);
        INTC->MSK |= INTC_MSK_IMSK25_I2C2_Msk;             /* interrupt source unmask (INTC) */
        NVIC_EnableIRQ(I2CB_IRQn);
    } else {
        NVIC_DisableIRQ(I2CB_IRQn);
        INTC->MSK &= ~INTC_MSK_IMSK25_I2C2_Msk;
        NVIC_ClearPendingIRQ(I2CB_IRQn);
    }
}

/* I2C block on with the SCL timing of s_un32Clock, interrupt request off until a transfer */
static void I2CB_HwSetup(void)
{
    uint32_t n   = I2CB_CalcPeriod(s_un32Clock);
    uint32_t low = (s_un32Clock > 100000U) ? ((n * 3U + 4U) / 5U)   /* Fm: tLOW > tHIGH */
                                           : ((n + 1U) / 2U);
    uint32_t high;

    if (low > I2CB_SCL_REG_MAX) {
        low = I2CB_SCL_REG_MAX;
    }
    if (low >= n) {
        low = n - 1U;
    }
    high = n - low;
    if (high > I2CB_SCL_REG_MAX) {
        high = I2CB_SCL_REG_MAX;
        low  = n - high;
    }

    SCUCG->PPCLKEN2 |= SCUCG_PPCLKEN2_I2C2CLKE_Msk;         /* I2C2 clock */
    I2CB->CR   = I2Cn_CR_I2CnEN_Msk;                        /* SDHR on, I2CnIEN off */
    I2CB->SCLR = low;
    I2CB->SCHR = high;
    I2CB->SDHR = low / 2U;                                  /* SDA changes mid SCL low (UM) */
    I2CB->SAR1 = 0U;
    I2CB->SAR2 = 0U;
    I2CB->ST   = I2CB_ST_CLEAR;
    s_bHeld    = 0U;
    s_xfer.state = I2CB_IDLE;
    I2CB_IrqEnable(true);
}

static void I2CB_HwOff(void)
{
    I2CB_IrqEnable(false);
    I2CB->CR = 0U;
    SCUCG->PPCLKEN2 &= ~SCUCG_PPCLKEN2_I2C2CLKE_Msk;
    s_bHeld = 0U;
    s_xfer.state = I2CB_IDLE;
}

static uint8_t I2CB_PinLevel(PCU_PIN_ID_e pin)
{
    PCU_PORT_e level = PCU_PORT_LOW;

    (void)HAL_PCU_GetInputValue(I2CB_PORT, pin, &level);
    return (level == PCU_PORT_HIGH) ? 1U : 0U;
}

static void I2CB_PinOut(PCU_PIN_ID_e pin, uint32_t high, uint32_t half)
{
    (void)HAL_PCU_SetOutputBit(I2CB_PORT, pin, high ? PCU_OUTPUT_BIT_SET : PCU_OUTPUT_BIT_CLEAR);
    I2CB_Delay(half);
}

/* Bus recovery: I2C block reset, up to 9 SCL pulses until the slave releases SDA, STOP */
static void I2CB_Recover(void)
{
    uint32_t half = TIMESTAMP_CLOCK / 200000U;              /* 100 kHz bit-bang */
    uint32_t i;

    I2CB_IrqEnable(false);
    I2CB->CR = 0U;
    SCUCG->PPRST2 |= SCUCG_PPRST2_I2C2RST_Msk;
    SCUCG->PPRST2 &= ~SCUCG_PPRST2_I2C2RST_Msk;

    (void)HAL_PCU_SetOutputBit(I2CB_PORT, I2CB_SCL_PIN, PCU_OUTPUT_BIT_SET);
    (void)HAL_PCU_SetOutputBit(I2CB_PORT, I2CB_SDA_PIN, PCU_OUTPUT_BIT_SET);
    (void)HAL_PCU_SetInOutMode(I2CB_PORT, I2CB_SCL_PIN, PCU_INOUT_OUTPUT_OPEN_DRAIN);
    (void)HAL_PCU_SetInOutMode(I2CB_PORT, I2CB_SDA_PIN, PCU_INOUT_OUTPUT_OPEN_DRAIN);
    I2CB_Delay(half);

    for (i = 0U; (i < 9U) && (I2CB_PinLevel(I2CB_SDA_PIN) == 0U); i++) {
        I2CB_PinOut(I2CB_SCL_PIN, 0U, half);
        I2CB_PinOut(I2CB_SCL_PIN, 1U, half);
    }
    I2CB_PinOut(I2CB_SCL_PIN, 0U, half);                   /* STOP: SDA low -> high while SCL high */
    I2CB_PinOut(I2CB_SDA_PIN, 0U, half);
    I2CB_PinOut(I2CB_SCL_PIN, 1U, half);
    I2CB_PinOut(I2CB_SDA_PIN, 1U, half);

    I2CB_SetPins(true);
    I2CB_HwSetup();
}

/* START (or the repeated START of a held bus) for the transfer loaded in s_xfer.
 * Caller: bus free or held, I2C interrupt request off. */
static void I2CB_Start(void)
{
    s_xfer.t0    = TIMESTAMP_GET();
    s_xfer.state = s_xfer.wphase ? I2CB_ADDR_W : I2CB_ADDR_R;
    if (!s_bHeld) {
        I2CB->ST = I2CB_ST_CLEAR;
    }
    I2CB->DR = s_xfer.wphase ? (uint32_t)(s_xfer.addr << 1) : (uint32_t)((s_xfer.addr << 1) | 1U);
    I2CB_CrUpdate(I2Cn_CR_STARTCn_Msk, 0U);
    if (s_bHeld) {
        s_bHeld  = 0U;
        I2CB->ST = I2CB_ST_CLEAR;                           /* release SCL: repeated START */
    }
    /* Interrupt on only now: the event left pending by a held transfer is cleared above */
    I2CB_CrUpdate(I2Cn_CR_I2CnIEN_Msk, 0U);
}

//**************************************************************************************************
// Stream ring buffer (producers: I2C2/EINT0 interrupts or code with IRQs masked)

static void Stream_Put(const uint8_t *p, uint32_t n)
{
    uint32_t head = s_stream.head;

    while (n--) {
        s_stream.buf[head++ & (STREAM_BUF_SIZE - 1U)] = *p++;
    }
    s_stream.head = head;
}

static void Stream_Push(uint8_t status, uint32_t ts, const uint8_t *data, uint32_t count)
{
    uint8_t hdr[STREAM_REC_HDR];

    if ((STREAM_BUF_SIZE - (s_stream.head - s_stream.tail)) < (STREAM_REC_HDR + count)) {
        if (s_stream.overrun != 0xFFFFU) {
            s_stream.overrun++;
        }
        return;
    }
    hdr[0] = status;
    hdr[1] = (uint8_t)count;
    I2CB_Put32(&hdr[2], ts);
    Stream_Put(hdr, STREAM_REC_HDR);                        /* head moves after the whole record */
    Stream_Put(data, count);
}

/* Start a stream transfer. Caller: IRQs masked or I2C2/EINT0 interrupt, I2C idle. */
static void Stream_Begin(uint32_t ts)
{
    uint32_t t0 = TIMESTAMP_GET();

    s_stream.ts = ts;
    while (I2CB->ST & I2Cn_ST_BUSYn_Msk) {                  /* right after the STOP of the last transfer */
        if ((TIMESTAMP_GET() - t0) >= (4U * I2CB_TBUF_TICKS)) {
            Stream_Push(I2C_STATUS_BUS_BUSY, ts, NULL, 0U); /* next trigger retries */
            return;
        }
    }
    s_xfer.addr   = s_stream.addr;
    s_xfer.flags  = s_stream.flags & I2C_FLAG_STOP_START;
    s_xfer.wphase = (s_stream.wlen != 0U) ? 1U : 0U;
    s_xfer.wbuf   = s_stream.wbuf;
    s_xfer.wlen   = s_stream.wlen;
    s_xfer.widx   = 0U;
    s_xfer.wcnt   = 0U;
    s_xfer.rbuf   = s_stream.rbuf;
    s_xfer.rlen   = s_stream.rlen;
    s_xfer.ridx   = 0U;
    s_xfer.status = I2C_STATUS_OK;
    s_xfer.done   = 0U;
    s_xfer.waiter = NULL;
    I2CB_Start();
}

/* Stream transfer finished (I2C2 interrupt): queue the record, run a pending trigger */
static void Stream_Done(void)
{
    Stream_Push(s_xfer.status, s_stream.ts, s_stream.rbuf, s_xfer.ridx);
    if (s_stream.active && s_stream.pending) {
        s_stream.pending = 0U;
        Stream_Begin(s_stream.pend_ts);
    }
}

/* Trigger (EINT0 interrupt or timer thread): start now, or once the running transfer ends */
static void Stream_Trigger(void)
{
    uint32_t ts = TIMESTAMP_GET();
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (s_stream.active) {
        if (s_xfer.state == I2CB_IDLE) {
            Stream_Begin(ts);
        } else {
            if (s_stream.pending && (s_stream.missed != 0xFFFFU)) {
                s_stream.missed++;                          /* merged with the pending trigger */
            }
            s_stream.pending = 1U;
            s_stream.pend_ts = ts;
        }
    }
    __set_PRIMASK(primask);
}

//**************************************************************************************************
// Interrupt state machine

/* Transfer finished: interrupt request off, wake the DAP command thread or queue the stream record */
static void I2CB_IsrDone(void)
{
    I2CB_CrUpdate(0U, I2Cn_CR_I2CnIEN_Msk);
    s_xfer.state = I2CB_IDLE;
    s_xfer.done  = 1U;
    if (s_xfer.waiter != NULL) {
        (void)osThreadFlagsSet(s_xfer.waiter, I2CB_THREAD_FLAG);
    } else {
        Stream_Done();
    }
}

/* Bus free after the STOP just released. Polled: the STOP event (STOPD) is sometimes not
 * flagged although the STOP is on the bus (about 1 in 300 transfers, BUSY clears). */
static bool I2CB_IsrWaitStop(void)
{
    uint32_t t0  = TIMESTAMP_GET();
    uint32_t tmo = (TIMESTAMP_CLOCK / s_un32Clock) * 4U;    /* STOP takes about 1.5 SCL periods */

    while (I2CB->ST & I2Cn_ST_BUSYn_Msk) {
        if ((TIMESTAMP_GET() - t0) >= tmo) {
            return false;                                   /* the STOP event ends the wait */
        }
    }
    return true;
}

/* START + SLA+R after the STOP of the write phase (bus free) */
static void I2CB_IsrStartRead(void)
{
    I2CB_Delay(I2CB_TBUF_TICKS);
    I2CB->ST = I2CB_ST_CLEAR;                               /* STOP event, if flagged */
    s_xfer.state = I2CB_ADDR_R;
    I2CB->DR = (uint32_t)((s_xfer.addr << 1) | 1U);
    I2CB_CrUpdate(I2Cn_CR_STARTCn_Msk, 0U);
}

/* STOP (from an event state), the transfer ends once the bus is free (or on the STOP event) */
static void I2CB_IsrStop(uint8_t status)
{
    s_xfer.status = status;
    s_xfer.state  = I2CB_STOP;
    I2CB_CrUpdate(I2Cn_CR_STOPCn_Msk, I2Cn_CR_ACKnEN_Msk);
    I2CB->ST = I2CB_ST_CLEAR;                               /* release SCL: STOP */
    if (I2CB_IsrWaitStop()) {
        I2CB->ST = I2CB_ST_CLEAR;                           /* STOP event, if flagged */
        I2CB_IsrDone();
    }
}

/* All data done: STOP, or keep the bus (SCL held low) for a repeated START */
static void I2CB_IsrEnd(void)
{
    if (s_xfer.flags & I2C_FLAG_NO_STOP) {
        s_bHeld = 1U;                                       /* event not cleared: SCL stays low */
        I2CB_IsrDone();
    } else {
        I2CB_IsrStop(I2C_STATUS_OK);
    }
}

/* Write phase done: repeated START (or STOP first) + SLA+R, or the end */
static void I2CB_IsrWriteDone(void)
{
    if (s_xfer.rlen == 0U) {
        I2CB_IsrEnd();
    } else if (s_xfer.flags & I2C_FLAG_STOP_START) {
        s_xfer.state = I2CB_STOP_RD;
        I2CB_CrUpdate(I2Cn_CR_STOPCn_Msk, I2Cn_CR_ACKnEN_Msk);
        I2CB->ST = I2CB_ST_CLEAR;                           /* release SCL: STOP */
        if (I2CB_IsrWaitStop()) {
            I2CB_IsrStartRead();
        }
    } else {
        s_xfer.state = I2CB_ADDR_R;
        I2CB->DR = (uint32_t)((s_xfer.addr << 1) | 1U);
        I2CB_CrUpdate(I2Cn_CR_STARTCn_Msk, 0U);
        I2CB->ST = I2CB_ST_CLEAR;                           /* release SCL: repeated START */
    }
}

/* Next received byte: ACK all but the last one, release SCL */
static void I2CB_IsrReadNext(void)
{
    if ((s_xfer.rlen - s_xfer.ridx) > 1U) {
        I2CB_CrUpdate(I2Cn_CR_ACKnEN_Msk, 0U);
    } else {
        I2CB_CrUpdate(0U, I2Cn_CR_ACKnEN_Msk);              /* NACK the last byte */
    }
    I2CB->ST = I2CB_ST_CLEAR;
}

/* I2C2 interrupt (IRQ 25). SCL is held low while an event bit is set. */
void Interrupt25_Handler(void)
{
    uint32_t st = I2CB->ST;

    if ((st & I2CB_EVT_Msk) == 0U) {
        return;
    }

    if ((st & I2Cn_ST_MLOSTn_Msk) && (s_xfer.state != I2CB_STOP) && (s_xfer.state != I2CB_STOP_RD)) {
        s_xfer.status = I2C_STATUS_ARB_LOST;                /* not master anymore: no STOP */
        I2CB->ST = I2CB_ST_CLEAR;
        I2CB_IsrDone();
        return;
    }

    switch (s_xfer.state) {
        case I2CB_ADDR_W:
            if ((st & I2Cn_ST_RXACKn_Msk) == 0U) {
                I2CB_IsrStop(I2C_STATUS_NACK_ADDR);
                break;
            }
            if (s_xfer.widx < s_xfer.wlen) {
                s_xfer.state = I2CB_WRITE;
                I2CB->DR = s_xfer.wbuf[s_xfer.widx++];
                I2CB->ST = I2CB_ST_CLEAR;                   /* release SCL: send the byte */
            } else {
                I2CB_IsrWriteDone();
            }
            break;

        case I2CB_WRITE:
            if ((st & I2Cn_ST_RXACKn_Msk) == 0U) {
                I2CB_IsrStop(I2C_STATUS_NACK_DATA);
                break;
            }
            s_xfer.wcnt++;
            if (s_xfer.widx < s_xfer.wlen) {
                I2CB->DR = s_xfer.wbuf[s_xfer.widx++];
                I2CB->ST = I2CB_ST_CLEAR;
            } else {
                I2CB_IsrWriteDone();
            }
            break;

        case I2CB_ADDR_R:
            if ((st & I2Cn_ST_RXACKn_Msk) == 0U) {
                I2CB_IsrStop(I2C_STATUS_NACK_ADDR);
                break;
            }
            s_xfer.state = I2CB_READ;
            I2CB_IsrReadNext();                             /* receive the first byte */
            break;

        case I2CB_READ:
            s_xfer.rbuf[s_xfer.ridx++] = (uint8_t)I2CB->DR;
            if (s_xfer.ridx < s_xfer.rlen) {
                I2CB_IsrReadNext();
            } else {
                I2CB_IsrEnd();
            }
            break;

        case I2CB_STOP:                                     /* STOP event: transfer over */
            I2CB->ST = I2CB_ST_CLEAR;
            I2CB_IsrDone();
            break;

        case I2CB_STOP_RD:                                  /* STOP event: START + SLA+R */
            I2CB_IsrStartRead();
            break;

        default:                                            /* no transfer: drop the event */
            I2CB->ST = I2CB_ST_CLEAR;
            I2CB_CrUpdate(0U, I2Cn_CR_I2CnIEN_Msk);
            break;
    }
}

//**************************************************************************************************
// Touch INT input (PA10, EINT0)

/* EINT0 callback (hal_pcu.c Interrupt3_Handler clears the port flags first) */
static void Tint_Handler(uint32_t un32Event, void *pContext)
{
    (void)pContext;
    if (un32Event & (1UL << TINT_PIN)) {
        Stream_Trigger();
    }
}

static void Tint_PinInit(void)
{
    if (!s_bTintInit) {
        (void)HAL_PCU_SetInOutMode(TINT_PORT, TINT_PIN, PCU_INOUT_INPUT);
        (void)HAL_PCU_SetPullUpDown(TINT_PORT, TINT_PIN, PCU_PUPD_UP);    /* open-drain INT outputs */
        s_bTintInit = 1U;
    }
}

static uint8_t Tint_Level(void)
{
    PCU_PORT_e level = PCU_PORT_LOW;

    Tint_PinInit();
    (void)HAL_PCU_GetInputValue(TINT_PORT, TINT_PIN, &level);
    return (level == PCU_PORT_HIGH) ? 1U : 0U;
}

/* Edge interrupt on PA10: ctl = INTC PACR INTCTL value (1 falling, 2 rising, 3 both), 0 = off */
static void Tint_IrqSetup(uint32_t ctl)
{
    PCU_IRQ_CFG_t cfg;

    NVIC_DisableIRQ(TINT_IRQn);
    INTC->PACR   = (INTC->PACR & ~INTC_PACR_INTCTL10_Msk);
    INTC->PAFLAG = INTC_PAFLAG_FLAG10_Msk;                  /* write 1 to clear */
    if (ctl == 0U) {
        INTC->MSK &= ~INTC_MSK_IMSK3_EINT0_Msk;
        NVIC_ClearPendingIRQ(TINT_IRQn);
        return;
    }

    Tint_PinInit();
    INTC->EINT0CONF2 = (INTC->EINT0CONF2 & ~INTC_EINT0CONF2_CONF10_Msk) |
                       ((uint32_t)INTC_EINT0CONF2_CONF10_PA << INTC_EINT0CONF2_CONF10_Pos);
    INTC->PATRIG &= ~INTC_PATRIG_ITRIG10_Msk;               /* edge */
    INTC->PACR   |= (ctl << INTC_PACR_INTCTL10_Pos) & INTC_PACR_INTCTL10_Msk;
    INTC->PAFLAG  = INTC_PAFLAG_FLAG10_Msk;

    cfg.eId         = TINT_PORT;
    cfg.eOps        = PCU_OPS_INTR;                         /* INTC unmask + NVIC on */
    cfg.pfnHandler  = Tint_Handler;
    cfg.pContext    = &s_tTintCtx;
    cfg.un32IRQPrio = TINT_IRQ_PRIO;
    cfg.un8IntNum   = TINT_EINT;
    (void)HAL_PCU_SetIRQ(&cfg);
}

//**************************************************************************************************
// Stream control (DAP command thread)

/* TIMER11 match (periodic mode) */
static void Stream_TickHandler(uint32_t un32Event, void *pContext)
{
    (void)pContext;
    if (un32Event & TIMER1_EVENT_PERIODIC_MATCH) {
        Stream_Trigger();
    }
}

/* Periodic trigger every 'period' ms, 0 = off */
static void Stream_TickSetup(uint32_t period)
{
    TIMER1_CLK_CFG_t clk;
    TIMER1_CFG_t cfg;

    if (period == 0U) {
        (void)HAL_TIMER1_Stop(STIM_ID);
        (void)HAL_TIMER1_SetIRQ(STIM_ID, TIMER1_OPS_POLL, NULL, NULL, 0U);
        (void)HAL_TIMER1_Uninit(STIM_ID);
        return;
    }

    memset(&clk, 0, sizeof(clk));
    clk.eClk         = TIMER1_CLK_PCLK;
    clk.un16PreScale = STIM_PRESCALE;
    memset(&cfg, 0, sizeof(cfg));
    cfg.eMode                = TIMER1_MODE_PERIODIC;
    cfg.bIntrEnable          = true;
    cfg.utData.tGRD.un16DataA = (uint16_t)(period * STIM_COUNTS_PER_MS - 1U);  /* match + clear: ADR + 1 counts */
    cfg.utData.tGRD.un16DataB = 0xFFFFU;

    (void)HAL_TIMER1_Init(STIM_ID);
    (void)HAL_TIMER1_SetClkConfig(STIM_ID, &clk);
    (void)HAL_TIMER1_SetConfig(STIM_ID, &cfg);
    (void)HAL_TIMER1_SetIRQ(STIM_ID, TIMER1_OPS_INTR, Stream_TickHandler, NULL, STIM_IRQ_PRIO);
    (void)HAL_TIMER1_Start(STIM_ID);
}

/* TIMESTAMP ticks for a transfer of 'bytes' data bytes: 2 x bus time + margin */
static uint32_t I2CB_TimeoutTs(uint32_t bytes, uint32_t margin)
{
    uint32_t bits = (bytes + 4U) * 9U;                      /* data + up to 2 addresses + START/STOPs */

    return (TIMESTAMP_CLOCK / s_un32Clock) * bits * 2U + margin;
}

/* Abort of a stuck stream transfer (SCL held low), called by StreamRead */
static void Stream_CheckStuck(void)
{
    uint32_t ts = 0U;
    uint8_t diag[STREAM_DIAG_LEN];
    bool stuck = false;

    __disable_irq();
    if (s_stream.active && (s_xfer.state != I2CB_IDLE) && (s_xfer.waiter == NULL) &&
        ((TIMESTAMP_GET() - s_xfer.t0) > I2CB_TimeoutTs(s_xfer.wlen + s_xfer.rlen, STREAM_MARGIN_TICKS))) {
        NVIC_DisableIRQ(I2CB_IRQn);                         /* the transfer cannot end any more */
        ts = s_stream.ts;
        stuck = true;
        /* TIMEOUT record data: where the transfer stopped (debug) */
        diag[0] = s_xfer.state;
        diag[1] = (uint8_t)I2CB->ST;
        diag[2] = (uint8_t)I2CB->CR;
        diag[3] = (uint8_t)s_xfer.widx;
        diag[4] = (uint8_t)s_xfer.ridx;
        diag[5] = I2CB_PinLevel(I2CB_SCL_PIN);
        diag[6] = I2CB_PinLevel(I2CB_SDA_PIN);
    }
    __enable_irq();

    if (stuck) {
        I2CB_Recover();                                     /* state IDLE, interrupt on again */
        __disable_irq();
        Stream_Push(I2C_STATUS_TIMEOUT, ts, diag, STREAM_DIAG_LEN);
        s_stream.pending = 0U;
        __enable_irq();
    }
}

static void Stream_Stop(void)
{
    uint32_t t0;

    if (!s_stream.active) {
        return;
    }
    Tint_IrqSetup(0U);
    if (s_stream.mode == STREAM_MODE_PERIODIC) {
        Stream_TickSetup(0U);
    }
    __disable_irq();
    s_stream.active  = 0U;
    s_stream.pending = 0U;
    __enable_irq();

    t0 = TIMESTAMP_GET();                                   /* last transfer: at most a few ms */
    while ((s_xfer.state != I2CB_IDLE) &&
           ((TIMESTAMP_GET() - t0) <= I2CB_TimeoutTs(s_xfer.wlen + s_xfer.rlen, STREAM_MARGIN_TICKS))) {
    }
    if (s_xfer.state != I2CB_IDLE) {
        I2CB_Recover();
    }
}

//**************************************************************************************************
// Transfer (DAP command thread)

/* Bus free check before a START. A bus stuck busy (slave holding SDA, missing STOP) is recovered. */
static uint8_t I2CB_WaitFree(void)
{
    uint32_t t0 = TIMESTAMP_GET();

    while (I2CB->ST & I2Cn_ST_BUSYn_Msk) {
        if ((TIMESTAMP_GET() - t0) >= I2CB_BUSY_TICKS) {
            I2CB_Recover();
            return (I2CB->ST & I2Cn_ST_BUSYn_Msk) ? I2C_STATUS_BUS_BUSY : I2C_STATUS_OK;
        }
    }
    return I2C_STATUS_OK;
}

/* [START + SLA+W + wdata] [repeated START (or STOP + START) + SLA+R + rdata], then STOP (or bus kept).
 * Write phase when wbuf != NULL or rlen == 0 (wlen 0 = address only), read phase when rlen != 0.
 * *pcount = data bytes ACKed (write only) or received (with a read phase). */
static uint8_t I2CB_Transfer(uint8_t addr, uint8_t flags, const uint8_t *wbuf, uint32_t wlen,
                             uint8_t *rbuf, uint32_t rlen, uint32_t *pcount)
{
    uint8_t  wphase = ((wbuf != NULL) || (rlen == 0U)) ? 1U : 0U;
    uint32_t tmo, elapsed;
    uint8_t  status;

    *pcount = 0U;
    if (!s_bHeld) {
        status = I2CB_WaitFree();
        if (status != I2C_STATUS_OK) {
            return status;
        }
    }

    s_xfer.addr   = addr;
    s_xfer.flags  = flags;
    s_xfer.wphase = wphase;
    s_xfer.wbuf   = wbuf;
    s_xfer.wlen   = wphase ? wlen : 0U;
    s_xfer.widx   = 0U;
    s_xfer.wcnt   = 0U;
    s_xfer.rbuf   = rbuf;
    s_xfer.rlen   = rlen;
    s_xfer.ridx   = 0U;
    s_xfer.status = I2C_STATUS_OK;
    s_xfer.done   = 0U;
    s_xfer.waiter = osThreadGetId();

    I2CB_Start();

    /* The event of an earlier, timed out transfer can still be set: s_xfer.done decides */
    tmo = I2CB_TimeoutTs(s_xfer.wlen + rlen, I2CB_MARGIN_TICKS);
    while (s_xfer.done == 0U) {
        elapsed = TIMESTAMP_GET() - s_xfer.t0;
        if (elapsed >= tmo) {
            break;
        }
        (void)osThreadFlagsWait(I2CB_THREAD_FLAG, osFlagsWaitAny,
                                (tmo - elapsed) / I2CB_OS_TICK_TS + 1U);
    }

    if (s_xfer.done == 0U) {
        I2CB_Recover();                                     /* interrupt off, bus freed */
        status = I2C_STATUS_TIMEOUT;
    } else {
        status = s_xfer.status;
    }

    *pcount = (rlen != 0U) ? s_xfer.ridx : s_xfer.wcnt;
    return status;
}

/* Common checks of the transfer commands */
static uint8_t I2CB_CheckReady(void)
{
    if (s_un32Clock == 0U) {
        return I2C_STATUS_NOT_CONFIG;
    }
    if (s_stream.active) {
        return I2C_STATUS_STREAMING;
    }
    return I2C_STATUS_OK;
}

//**************************************************************************************************
// Commands

static uint32_t I2C_Config(const uint8_t *request, uint8_t *response)
{
    uint32_t hz = (uint32_t)request[0] | ((uint32_t)request[1] << 8) |
                  ((uint32_t)request[2] << 16) | ((uint32_t)request[3] << 24);

    if ((hz != 0U) && (I2CB_CalcPeriod(hz) == 0U)) {
        response[0] = I2C_STATUS_PARAM;
        I2CB_Put32(&response[1], s_un32Clock ? I2CB_ActualClock() : 0U);
        return 5U;
    }

    Stream_Stop();
    if (s_bHeld) {
        I2CB_Recover();                                     /* end a held transfer with a STOP */
    }

    if (hz == 0U) {
        if (s_un32Clock != 0U) {
            I2CB_HwOff();
            I2CB_SetPins(false);
            s_un32Clock = 0U;
        }
        response[0] = I2C_STATUS_OK;
        I2CB_Put32(&response[1], 0U);
        return 5U;
    }

    s_un32Clock = hz;
    I2CB_SetPins(true);
    I2CB_HwSetup();

    response[0] = I2C_STATUS_OK;
    I2CB_Put32(&response[1], I2CB_ActualClock());
    return 5U;
}

static uint32_t I2C_Write(const uint8_t *request, uint8_t *response, uint32_t size, uint32_t *preq)
{
    uint32_t len = I2CB_Get16(&request[2]);
    uint32_t count = 0U;
    uint8_t status = I2CB_CheckReady();

    *preq = 4U + I2CB_Min(len, I2C_WRITE_MAX(size));
    if (status != I2C_STATUS_OK) {
        /* not ready */
    } else if ((request[0] > 0x7FU) || (len > I2C_WRITE_MAX(size))) {
        status = I2C_STATUS_PARAM;
    } else {
        status = I2CB_Transfer(request[0], request[1], &request[4], len, NULL, 0U, &count);
    }
    response[0] = status;
    I2CB_Put16(&response[1], count);
    return 3U;
}

static uint32_t I2C_Read(const uint8_t *request, uint8_t *response, uint32_t size)
{
    uint32_t len = I2CB_Get16(&request[2]);
    uint32_t count = 0U;
    uint8_t status = I2CB_CheckReady();

    if (status != I2C_STATUS_OK) {
        /* not ready */
    } else if ((request[0] > 0x7FU) || (len == 0U) || (len > I2C_READ_MAX(size))) {
        status = I2C_STATUS_PARAM;
    } else {
        status = I2CB_Transfer(request[0], request[1], NULL, 0U, &response[3], len, &count);
    }
    response[0] = status;
    I2CB_Put16(&response[1], count);
    return 3U + count;
}

static uint32_t I2C_WriteRead(const uint8_t *request, uint8_t *response, uint32_t size, uint32_t *preq)
{
    uint32_t wlen = I2CB_Get16(&request[2]);
    uint32_t rlen = I2CB_Get16(&request[4]);
    uint32_t count = 0U;
    uint8_t status = I2CB_CheckReady();

    *preq = 6U + I2CB_Min(wlen, I2C_WR_WRITE_MAX(size));
    if (status != I2C_STATUS_OK) {
        /* not ready */
    } else if ((request[0] > 0x7FU) || (wlen > I2C_WR_WRITE_MAX(size)) ||
               (rlen == 0U) || (rlen > I2C_READ_MAX(size))) {
        status = I2C_STATUS_PARAM;
    } else {
        status = I2CB_Transfer(request[0], request[1], &request[6], wlen, &response[3], rlen, &count);
    }
    response[0] = status;
    I2CB_Put16(&response[1], count);
    return 3U + count;
}

static uint32_t I2C_Scan(uint8_t *response)
{
    uint32_t addr;
    uint32_t count;
    uint8_t status = I2CB_CheckReady();
    uint8_t result;

    memset(&response[1], 0, 16U);
    if (status == I2C_STATUS_OK) {
        for (addr = 0x08U; addr <= 0x77U; addr++) {
            result = I2CB_Transfer((uint8_t)addr, 0U, NULL, 0U, NULL, 0U, &count);
            if (result == I2C_STATUS_OK) {
                response[1U + (addr >> 3)] |= (uint8_t)(1U << (addr & 7U));
            } else if (result != I2C_STATUS_NACK_ADDR) {
                status = result;                            /* bus problem: stop scanning */
                break;
            }
        }
    }
    response[0] = status;
    return 17U;
}

static uint32_t I2C_IntState(uint8_t *response)
{
    response[0] = I2C_STATUS_OK;
    response[1] = Tint_Level();
    return 2U;
}

static uint32_t I2C_StreamStart(const uint8_t *request, uint8_t *response, uint32_t *preq)
{
    uint8_t  addr   = request[0];
    uint8_t  flags  = request[1];
    uint8_t  mode   = request[2];
    uint32_t period = I2CB_Get16(&request[3]);
    uint32_t wlen   = request[5];
    uint32_t rlen   = request[6];
    uint8_t  status = I2CB_CheckReady();

    *preq = 7U + I2CB_Min(wlen, STREAM_WLEN_MAX);
    I2CB_Put32(&response[1], TIMESTAMP_CLOCK);
    if (status != I2C_STATUS_OK) {
        response[0] = status;
        return 5U;
    }
    if ((addr > 0x7FU) || (mode > STREAM_MODE_INT_BOTH) || (wlen > STREAM_WLEN_MAX) ||
        (rlen == 0U) || (rlen > STREAM_RLEN_MAX) ||
        ((mode == STREAM_MODE_PERIODIC) && ((period == 0U) || (period > STREAM_PERIOD_MAX)))) {
        response[0] = I2C_STATUS_PARAM;
        return 5U;
    }
    if (s_bHeld) {
        I2CB_Recover();                                     /* end a held transfer with a STOP */
    }

    s_stream.addr    = addr;
    s_stream.flags   = flags;
    s_stream.mode    = mode;
    s_stream.wlen    = (uint8_t)wlen;
    s_stream.rlen    = (uint8_t)rlen;
    memcpy(s_stream.wbuf, &request[7], wlen);
    s_stream.overrun = 0U;
    s_stream.missed  = 0U;
    s_stream.pending = 0U;
    s_stream.tail    = s_stream.head;                       /* drop old records */
    s_stream.active  = 1U;

    if (mode == STREAM_MODE_PERIODIC) {
        Stream_TickSetup(period);
    } else {
        Tint_IrqSetup(mode);                                /* INTCTL: 1 falling, 2 rising, 3 both */
    }

    response[0] = I2C_STATUS_OK;
    return 5U;
}

static uint32_t I2C_StreamRead(uint8_t *response, uint32_t size)
{
    uint32_t space = size - 1U - STREAM_RSP_HDR;
    uint32_t tail  = s_stream.tail;
    uint32_t head  = s_stream.head;
    uint32_t num   = STREAM_RSP_HDR;
    uint32_t n     = 0U;
    uint32_t rec, i;

    Stream_CheckStuck();
    head = s_stream.head;

    while ((head - tail) >= STREAM_REC_HDR) {
        rec = STREAM_REC_HDR + s_stream.buf[(tail + 1U) & (STREAM_BUF_SIZE - 1U)];
        if ((rec > space) || (n == 0xFFU)) {
            break;
        }
        for (i = 0U; i < rec; i++) {
            response[num++] = s_stream.buf[tail++ & (STREAM_BUF_SIZE - 1U)];
        }
        space -= rec;
        n++;
    }
    s_stream.tail = tail;

    __disable_irq();
    I2CB_Put16(&response[1], s_stream.overrun);
    I2CB_Put16(&response[3], s_stream.missed);
    s_stream.overrun = 0U;
    s_stream.missed  = 0U;
    __enable_irq();

    response[0] = (s_un32Clock == 0U) ? I2C_STATUS_NOT_CONFIG : I2C_STATUS_OK;
    response[5] = (uint8_t)n;
    return num;
}

//**************************************************************************************************

// Execute one I2C command
//   request and response can be the same buffer (I2C USB interface): each command reads its
//   request fields before it writes the response over them.
//   size  : packet size (request and response limit)
//   return: number of bytes in response (lower 16 bits)
//           number of bytes in request (upper 16 bits)
static uint32_t I2C_Execute(const uint8_t *request, uint8_t *response, uint32_t size)
{
    uint32_t num;
    uint32_t req = 0U;

    *response++ = *request;

    switch (*request++) {
        case ID_DAP_I2C_Config:
            num = I2C_Config(request, response);
            req = 4U;
            break;

        case ID_DAP_I2C_Write:
            num = I2C_Write(request, response, size, &req);
            break;

        case ID_DAP_I2C_Read:
            num = I2C_Read(request, response, size);
            req = 4U;
            break;

        case ID_DAP_I2C_WriteRead:
            num = I2C_WriteRead(request, response, size, &req);
            break;

        case ID_DAP_I2C_Scan:
            num = I2C_Scan(response);
            break;

        case ID_DAP_I2C_IntState:
            num = I2C_IntState(response);
            break;

        case ID_DAP_I2C_StreamStart:
            num = I2C_StreamStart(request, response, &req);
            break;

        case ID_DAP_I2C_StreamRead:
            num = I2C_StreamRead(response, size);
            break;

        case ID_DAP_I2C_StreamStop:
            Stream_Stop();
            response[0] = I2C_STATUS_OK;
            num = 1U;
            break;

        default:
            *(response - 1) = ID_DAP_Invalid;
            return ((1U << 16) | 1U);
    }

    return (((1U + req) << 16) | (1U + num));
}

// Process DAP vendor extended command (overrides the __WEAK default in DAP.c)
uint32_t DAP_ProcessVendorCommandEx(const uint8_t *request, uint8_t *response)
{
    return I2C_Execute(request, response, DAP_PACKET_SIZE);
}

#if defined(BULK_ENDPOINT)
//**************************************************************************************************
// I2C USB interface (usb_config.c USBD_I2C_xxx, EP5 bulk): one command per transfer, same format as
// the DAP vendor commands. The host sends the next request only after it has read the response, so
// one buffer holds the request and then the response. A request packet that comes while the response
// is still being sent is left in the endpoint FIFO (NAKed) and read when the response is done.
// The commands run in the USB event context (main task), between the CMSIS-DAP commands.

static struct {
    uint32_t            buf[I2CU_PACKET_SIZE / 4U];     ///< word aligned: the USB FIFO copy is 32 bit
    uint32_t            len;                            ///< request bytes received
    volatile uint8_t    busy;                           ///< response being sent
    volatile uint8_t    pending;                        ///< OUT packet left in the FIFO while busy
} s_usb;

void usbd_i2c_init(void)
{
    s_usb.len     = 0U;
    s_usb.busy    = 0U;
    s_usb.pending = 0U;
}

static void I2CU_Out(void)
{
    uint8_t *buf = (uint8_t *)s_usb.buf;
    uint32_t n;

    if (s_usb.busy) {
        s_usb.pending = 1U;
        return;
    }
    s_usb.pending = 0U;

    n = USBD_ReadEP(usbd_i2c_ep_bulkout, &buf[s_usb.len], I2CU_PACKET_SIZE - s_usb.len);
    s_usb.len += n;
    if ((n == I2CU_FS_PACKET) && (s_usb.len < I2CU_PACKET_SIZE)) {
        return;                                         /* more packets follow (short packet / ZLP ends) */
    }
    if (s_usb.len == 0U) {
        return;                                         /* stray zero-length packet */
    }

    n = I2C_Execute(buf, buf, I2CU_PACKET_SIZE) & 0xFFFFU;
    s_usb.len  = 0U;
    s_usb.busy = 1U;
    USBD_WriteEP(usbd_i2c_ep_bulkin | 0x80U, buf, n);   /* ZLP added by the driver on a packet boundary */
}

void USBD_I2C_EP_BULK_Event(U32 event)
{
    if (event & USBD_EVT_OUT) {
        I2CU_Out();
    }
    if (event & USBD_EVT_IN) {
        s_usb.busy = 0U;
        if (s_usb.pending) {
            I2CU_Out();
        }
    }
}
#endif
