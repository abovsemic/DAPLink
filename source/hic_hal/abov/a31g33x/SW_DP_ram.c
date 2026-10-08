/**
 * @file    SW_DP_ram.c
 * @brief   A31G33x SWD I/O (replaces the __WEAK functions of cmsis-dap/SW_DP.c)
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 *
 * Based on CMSIS-DAP SW_DP.c V2.0.0 (Copyright (c) 2013-2017 ARM Limited).
 * Differences (see swj_a31g33x.h):
 *  - runs from RAM ("ram_func"), flash needs 2-wait at 48MHz
 *  - clock band from DAP_Data.nominal_clock: FAST1 / FAST2 (fixed NOPs) / TIMED (TIMER2n)
 *  - read bits have their own low/high half delays (RD / RD_HI)
 *  - FAST 32-bit data phase unrolled, SWDIO write and direction switch without branch/HAL call
 */

#include "swj_a31g33x.h"

#if defined(__CC_ARM) && ((DAP_SWD != 0) || (DAP_JTAG != 0))

#pragma push
#pragma O3
#pragma Otime

// SW Macros (PIN_DELAY / PIN_DELAY_RD / PIN_DELAY_RD_HI / SWJ_INIT set per band below)

#define SW_CLOCK_CYCLE()                \
  PIN_SWCLK_TCK_CLR();                  \
  PIN_DELAY();                          \
  PIN_SWCLK_TCK_SET();                  \
  PIN_DELAY()

#define SW_WRITE_BIT(bit)               \
  SWJ_SWDIO_OUT(bit);                   \
  PIN_SWCLK_TCK_CLR();                  \
  PIN_DELAY();                          \
  PIN_SWCLK_TCK_SET();                  \
  PIN_DELAY_WR_HI()

#define SW_READ_BIT(bit)                \
  PIN_SWCLK_TCK_CLR();                  \
  PIN_DELAY_RD();                       \
  bit = PIN_SWDIO_IN();                 \
  PIN_SWCLK_TCK_SET();                  \
  PIN_DELAY_RD_HI()


//**************************************************************************************************
// TIMED band

#if (SWJ_TIMED_IN_RAM != 0)
#pragma arm section code = "ram_func"
#endif

SWJ_TIMED_WAIT_FUNCTION()

#define SWJ_INIT()         SWJ_TIMED_INIT()
#define PIN_DELAY()         SWJ_TIMED_DELAY()
#define PIN_DELAY_RD()      SWJ_TIMED_DELAY()
#define PIN_DELAY_RD_HI()   SWJ_TIMED_DELAY()
#define PIN_DELAY_WR_HI()   SWJ_TIMED_DELAY()


// Generate SWJ Sequence
//   count:  sequence bit count
//   data:   pointer to sequence bit data
//   return: none
void SWJ_Sequence (uint32_t count, const uint8_t *data) {
  uint32_t val;
  uint32_t n;
  SWJ_CheckClock();
  {
  SWJ_INIT();

  val = 0U;
  n = 0U;
  while (count--) {
    if (n == 0U) {
      val = *data++;
      n = 8U;
    }
    if (val & 1U) {
      PIN_SWDIO_TMS_SET();
    } else {
      PIN_SWDIO_TMS_CLR();
    }
    SW_CLOCK_CYCLE();
    val >>= 1;
    n--;
  }
  }
}


#if (DAP_SWD != 0)

// Generate SWD Sequence
//   info:   sequence information
//   swdo:   pointer to SWDIO generated data
//   swdi:   pointer to SWDIO captured data
//   return: none
void SWD_Sequence (uint32_t info, const uint8_t *swdo, uint8_t *swdi) {
  uint32_t val;
  uint32_t bit;
  uint32_t n, k;
  SWJ_CheckClock();
  {
  SWJ_INIT();

  n = info & SWD_SEQUENCE_CLK;
  if (n == 0U) {
    n = 64U;
  }

  if (info & SWD_SEQUENCE_DIN) {
    while (n) {
      val = 0U;
      for (k = 8U; k && n; k--, n--) {
        SW_READ_BIT(bit);
        val >>= 1;
        val  |= bit << 7;
      }
      val >>= k;
      *swdi++ = (uint8_t)val;
    }
  } else {
    while (n) {
      val = *swdo++;
      for (k = 8U; k && n; k--, n--) {
        SW_WRITE_BIT(val);
        val >>= 1;
      }
    }
  }
  }
}


// 32-bit data phase (uses the locals val, bit, n of SWD_TransferFunction).
// Parity is not accumulated per bit (SWJ_Parity32 once per word, see swj_a31g33x.h).
#define SWD_READ_BIT32_BODY()                                                   \
  SW_READ_BIT(bit);                                                             \
  val >>= 1;                                                                    \
  val  |= bit << 31

#define SWD_WRITE_BIT32_BODY()                                                  \
  SW_WRITE_BIT(val);                                                            \
  val >>= 1

#define SWD_READ_DATA32()                                                       \
  for (n = 32U; n; n--) {                                                       \
    SWD_READ_BIT32_BODY();                                                      \
  }

#define SWD_WRITE_DATA32()                                                      \
  for (n = 32U; n; n--) {                                                       \
    SWD_WRITE_BIT32_BODY();                                                     \
  }


// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
#define SWD_TransferFunction(speed)     /**/                                    \
static uint8_t SWD_Transfer##speed (uint32_t request, uint32_t *data) {         \
  uint32_t ack;                                                                 \
  uint32_t bit;                                                                 \
  uint32_t val;                                                                 \
  uint32_t parity;                                                              \
                                                                                \
  uint32_t n;                                                                   \
  SWJ_INIT();                                                                   \
                                                                                \
  /* Packet Request */                                                          \
  parity = 0U;                                                                  \
  SW_WRITE_BIT(1U);                     /* Start Bit */                         \
  bit = request >> 0;                                                           \
  SW_WRITE_BIT(bit);                    /* APnDP Bit */                         \
  parity += bit;                                                                \
  bit = request >> 1;                                                           \
  SW_WRITE_BIT(bit);                    /* RnW Bit */                           \
  parity += bit;                                                                \
  bit = request >> 2;                                                           \
  SW_WRITE_BIT(bit);                    /* A2 Bit */                            \
  parity += bit;                                                                \
  bit = request >> 3;                                                           \
  SW_WRITE_BIT(bit);                    /* A3 Bit */                            \
  parity += bit;                                                                \
  SW_WRITE_BIT(parity);                 /* Parity Bit */                        \
  SW_WRITE_BIT(0U);                     /* Stop Bit */                          \
  SW_WRITE_BIT(1U);                     /* Park Bit */                          \
                                                                                \
  /* Turnaround */                                                              \
  SWJ_SWDIO_OUT_DISABLE();                                                      \
  for (n = DAP_Data.swd_conf.turnaround; n; n--) {                              \
    SW_CLOCK_CYCLE();                                                           \
  }                                                                             \
                                                                                \
  /* Acknowledge response */                                                    \
  SW_READ_BIT(bit);                                                             \
  ack  = bit << 0;                                                              \
  SW_READ_BIT(bit);                                                             \
  ack |= bit << 1;                                                              \
  SW_READ_BIT(bit);                                                             \
  ack |= bit << 2;                                                              \
                                                                                \
  if (ack == DAP_TRANSFER_OK) {         /* OK response */                       \
    /* Data transfer */                                                         \
    if (request & DAP_TRANSFER_RnW) {                                           \
      /* Read data */                                                           \
      val = 0U;                                                                 \
      SWD_READ_DATA32();                /* Read RDATA[0:31] */                  \
      SW_READ_BIT(bit);                 /* Read Parity */                       \
      if ((SWJ_Parity32(val) ^ bit) & 1U) {                                     \
        ack = DAP_TRANSFER_ERROR;                                               \
      }                                                                         \
      if (data) { *data = val; }                                                \
      /* Turnaround */                                                          \
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {                          \
        SW_CLOCK_CYCLE();                                                       \
      }                                                                         \
      SWJ_SWDIO_OUT_ENABLE();                                                   \
    } else {                                                                    \
      val = *data;                                                              \
      parity = SWJ_Parity32(val);       /* before the bits, not per bit */      \
      /* Turnaround */                                                          \
      for (n = DAP_Data.swd_conf.turnaround; n; n--) {                          \
        SW_CLOCK_CYCLE();                                                       \
      }                                                                         \
      SWJ_SWDIO_OUT_ENABLE();                                                   \
      /* Write data */                                                          \
      SWD_WRITE_DATA32();               /* Write WDATA[0:31] */                 \
      SW_WRITE_BIT(parity);             /* Write Parity Bit */                  \
    }                                                                           \
    /* Capture Timestamp */                                                     \
    if (request & DAP_TRANSFER_TIMESTAMP) {                                     \
      DAP_Data.timestamp = TIMESTAMP_GET();                                     \
    }                                                                           \
    /* Idle cycles */                                                           \
    n = DAP_Data.transfer.idle_cycles;                                          \
    if (n) {                                                                    \
      SWJ_SWDIO_OUT(0U);                                                        \
      for (; n; n--) {                                                          \
        SW_CLOCK_CYCLE();                                                       \
      }                                                                         \
    }                                                                           \
    SWJ_SWDIO_OUT(1U);                                                          \
    return ((uint8_t)ack);                                                      \
  }                                                                             \
                                                                                \
  if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {              \
    /* WAIT or FAULT response */                                                \
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) { \
      for (n = 32U+1U; n; n--) {                                                \
        SW_CLOCK_CYCLE();               /* Dummy Read RDATA[0:31] + Parity */   \
      }                                                                         \
    }                                                                           \
    /* Turnaround */                                                            \
    for (n = DAP_Data.swd_conf.turnaround; n; n--) {                            \
      SW_CLOCK_CYCLE();                                                         \
    }                                                                           \
    SWJ_SWDIO_OUT_ENABLE();                                                     \
    if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) { \
      SWJ_SWDIO_OUT(0U);                                                        \
      for (n = 32U+1U; n; n--) {                                                \
        SW_CLOCK_CYCLE();               /* Dummy Write WDATA[0:31] + Parity */  \
      }                                                                         \
    }                                                                           \
    SWJ_SWDIO_OUT(1U);                                                          \
    return ((uint8_t)ack);                                                      \
  }                                                                             \
                                                                                \
  /* Protocol error */                                                          \
  for (n = DAP_Data.swd_conf.turnaround + 32U + 1U; n; n--) {                   \
    SW_CLOCK_CYCLE();                   /* Back off data phase */               \
  }                                                                             \
  SWJ_SWDIO_OUT_ENABLE();                                                       \
  SWJ_SWDIO_OUT(1U);                                                            \
  return ((uint8_t)ack);                                                        \
}


SWD_TransferFunction(Timed)

#endif  /* (DAP_SWD != 0) */


//**************************************************************************************************
// FAST bands (always in RAM)

#pragma arm section code = "ram_func"

#if (DAP_SWD != 0)

#undef  SWJ_INIT
#define SWJ_INIT()

#if (SWJ_FAST_UNROLL != 0)
// Every data bit runs the same straight-line code as the request bits (no loop branch)
#undef  SWD_READ_DATA32
#undef  SWD_WRITE_DATA32
#define SWD_READ_DATA32()   do { SWJ_REP32(SWD_READ_BIT32_BODY()); } while (0)
#define SWD_WRITE_DATA32()  do { SWJ_REP32(SWD_WRITE_BIT32_BODY()); } while (0)
#endif

#undef  PIN_DELAY
#undef  PIN_DELAY_RD
#undef  PIN_DELAY_RD_HI
#undef  PIN_DELAY_WR_HI
#define PIN_DELAY()         SWJ_FAST_DELAY(SWD_FAST1_NOPS)
#define PIN_DELAY_RD()      SWJ_FAST_DELAY(SWD_FAST1_RD_NOPS)
#define PIN_DELAY_RD_HI()   SWJ_FAST_DELAY(SWD_FAST1_RD_HI_NOPS)
#define PIN_DELAY_WR_HI()   SWJ_FAST_DELAY(SWD_FAST1_WR_HI_NOPS)
SWD_TransferFunction(Fast1)

#if (SWJ_FAST2_ENABLE != 0)
#undef  PIN_DELAY
#undef  PIN_DELAY_RD
#undef  PIN_DELAY_RD_HI
#undef  PIN_DELAY_WR_HI
#define PIN_DELAY()         SWJ_FAST_DELAY(SWD_FAST2_NOPS)
#define PIN_DELAY_RD()      SWJ_FAST_DELAY(SWD_FAST2_RD_NOPS)
#define PIN_DELAY_RD_HI()   SWJ_FAST_DELAY(SWD_FAST2_RD_HI_NOPS)
#define PIN_DELAY_WR_HI()   SWJ_FAST_DELAY(SWD_FAST2_WR_HI_NOPS)
SWD_TransferFunction(Fast2)
#endif


// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
uint8_t SWD_Transfer (uint32_t request, uint32_t *data) {
  SWJ_CheckClock();
  switch (swd_band) {
    case SWJ_BAND_FAST1: return SWD_TransferFast1(request, data);
#if (SWJ_FAST2_ENABLE != 0)
    case SWJ_BAND_FAST2: return SWD_TransferFast2(request, data);
#endif
    default:             return SWD_TransferTimed(request, data);
  }
}

#endif  /* (DAP_SWD != 0) */

#pragma arm section code

#pragma pop

#endif  /* __CC_ARM */
