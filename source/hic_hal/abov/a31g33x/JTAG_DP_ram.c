/**
 * @file    JTAG_DP_ram.c
 * @brief   A31G33x JTAG I/O (replaces the functions of cmsis-dap/JTAG_DP.c)
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 *
 * Based on CMSIS-DAP JTAG_DP.c V2.0.0 (Copyright (c) 2013-2017 ARM Limited).
 * The functions of JTAG_DP.c are not __WEAK, so they are defined here as $Sub$$JTAG_xxx:
 * armlink redirects every call to JTAG_xxx from other objects (DAP.c) to $Sub$$JTAG_xxx and
 * removes the unused originals. JTAG_DP.c itself is left untouched.
 *
 * Differences (see swj_a31g33x.h):
 *  - runs from RAM ("ram_func"), flash needs 2-wait at 48MHz
 *  - clock band from DAP_Data.nominal_clock: FAST1 / FAST2 (fixed NOPs) / TIMED (TIMER2n)
 *  - TDO read bits have their own low/high half delays (RD / RD_HI): the target changes TDO
 *    on the TCK falling edge, so only the low half before the TDO read needs to be longer
 *  - FAST 31-bit data phase unrolled, TDI write without branch
 */

#include "swj_a31g33x.h"      /* includes DAP.h: original prototypes before the renames */

#if defined(__CC_ARM) && (DAP_JTAG != 0)

#define JTAG_Sequence       $Sub$$JTAG_Sequence
#define JTAG_ReadIDCode     $Sub$$JTAG_ReadIDCode
#define JTAG_WriteAbort     $Sub$$JTAG_WriteAbort
#define JTAG_IR             $Sub$$JTAG_IR
#define JTAG_Transfer       $Sub$$JTAG_Transfer

#pragma push
#pragma O3
#pragma Otime

// JTAG Macros (PIN_DELAY / PIN_DELAY_RD / PIN_DELAY_RD_HI / SWJ_INIT set per band below)

#define PIN_TCK_SET PIN_SWCLK_TCK_SET
#define PIN_TCK_CLR PIN_SWCLK_TCK_CLR
#define PIN_TMS_SET PIN_SWDIO_TMS_SET
#define PIN_TMS_CLR PIN_SWDIO_TMS_CLR

#define JTAG_CYCLE_TCK()                \
  PIN_TCK_CLR();                        \
  PIN_DELAY();                          \
  PIN_TCK_SET();                        \
  PIN_DELAY()

#define JTAG_CYCLE_TDI(tdi)             \
  SWJ_TDI_OUT(tdi);                     \
  PIN_TCK_CLR();                        \
  PIN_DELAY();                          \
  PIN_TCK_SET();                        \
  PIN_DELAY_WR_HI()

#define JTAG_CYCLE_TDO(tdo)             \
  PIN_TCK_CLR();                        \
  PIN_DELAY_RD();                       \
  tdo = PIN_TDO_IN();                   \
  PIN_TCK_SET();                        \
  PIN_DELAY_RD_HI()

#define JTAG_CYCLE_TDIO(tdi,tdo)        \
  SWJ_TDI_OUT(tdi);                     \
  PIN_TCK_CLR();                        \
  PIN_DELAY_RD();                       \
  tdo = PIN_TDO_IN();                   \
  PIN_TCK_SET();                        \
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


// Generate JTAG Sequence
//   info:   sequence information
//   tdi:    pointer to TDI generated data
//   tdo:    pointer to TDO captured data
//   return: none
void JTAG_Sequence (uint32_t info, const uint8_t *tdi, uint8_t *tdo) {
  uint32_t i_val;
  uint32_t o_val;
  uint32_t bit;
  uint32_t n, k;
  SWJ_CheckClock();
  {
  SWJ_INIT();

  n = info & JTAG_SEQUENCE_TCK;
  if (n == 0U) {
    n = 64U;
  }

  if (info & JTAG_SEQUENCE_TMS) {
    PIN_TMS_SET();
  } else {
    PIN_TMS_CLR();
  }

  while (n) {
    i_val = *tdi++;
    o_val = 0U;
    for (k = 8U; k && n; k--, n--) {
      JTAG_CYCLE_TDIO(i_val, bit);
      i_val >>= 1;
      o_val >>= 1;
      o_val  |= bit << 7;
    }
    o_val >>= k;
    if (info & JTAG_SEQUENCE_TDO) {
      *tdo++ = (uint8_t)o_val;
    }
  }
  }
}


// JTAG Read IDCODE register
//   return: value read
uint32_t JTAG_ReadIDCode (void) {
  uint32_t bit;
  uint32_t val;
  uint32_t n;
  SWJ_CheckClock();
  {
  SWJ_INIT();

  PIN_TMS_SET();
  JTAG_CYCLE_TCK();                         /* Select-DR-Scan */
  PIN_TMS_CLR();
  JTAG_CYCLE_TCK();                         /* Capture-DR */
  JTAG_CYCLE_TCK();                         /* Shift-DR */

  for (n = DAP_Data.jtag_dev.index; n; n--) {
    JTAG_CYCLE_TCK();                       /* Bypass before data */
  }

  val = 0U;
  for (n = 31U; n; n--) {
    JTAG_CYCLE_TDO(bit);                    /* Get D0..D30 */
    val  |= bit << 31;
    val >>= 1;
  }
  PIN_TMS_SET();
  JTAG_CYCLE_TDO(bit);                      /* Get D31 & Exit1-DR */
  val |= bit << 31;

  JTAG_CYCLE_TCK();                         /* Update-DR */
  PIN_TMS_CLR();
  JTAG_CYCLE_TCK();                         /* Idle */

  return (val);
  }
}


// JTAG Write ABORT register
//   data:   value to write
//   return: none
void JTAG_WriteAbort (uint32_t data) {
  uint32_t n;
  SWJ_CheckClock();
  {
  SWJ_INIT();

  PIN_TMS_SET();
  JTAG_CYCLE_TCK();                         /* Select-DR-Scan */
  PIN_TMS_CLR();
  JTAG_CYCLE_TCK();                         /* Capture-DR */
  JTAG_CYCLE_TCK();                         /* Shift-DR */

  for (n = DAP_Data.jtag_dev.index; n; n--) {
    JTAG_CYCLE_TCK();                       /* Bypass before data */
  }

  SWJ_TDI_OUT(0U);
  JTAG_CYCLE_TCK();                         /* Set RnW=0 (Write) */
  JTAG_CYCLE_TCK();                         /* Set A2=0 */
  JTAG_CYCLE_TCK();                         /* Set A3=0 */

  for (n = 31U; n; n--) {
    JTAG_CYCLE_TDI(data);                   /* Set D0..D30 */
    data >>= 1;
  }
  n = DAP_Data.jtag_dev.count - DAP_Data.jtag_dev.index - 1U;
  if (n) {
    JTAG_CYCLE_TDI(data);                   /* Set D31 */
    for (--n; n; n--) {
      JTAG_CYCLE_TCK();                     /* Bypass after data */
    }
    PIN_TMS_SET();
    JTAG_CYCLE_TCK();                       /* Bypass & Exit1-DR */
  } else {
    PIN_TMS_SET();
    JTAG_CYCLE_TDI(data);                   /* Set D31 & Exit1-DR */
  }

  JTAG_CYCLE_TCK();                         /* Update-DR */
  PIN_TMS_CLR();
  JTAG_CYCLE_TCK();                         /* Idle */
  SWJ_TDI_OUT(1U);
  }
}


// 31-bit data phase D0..D30 (uses the locals val, bit, n of JTAG_TransferFunction)
#define JTAG_READ_BIT31_BODY()                                                  \
  JTAG_CYCLE_TDO(bit);                                                          \
  val  |= bit << 31;                                                            \
  val >>= 1

#define JTAG_WRITE_BIT31_BODY()                                                 \
  JTAG_CYCLE_TDI(val);                                                          \
  val >>= 1

#define JTAG_READ_DATA31()                                                      \
  for (n = 31U; n; n--) {                                                       \
    JTAG_READ_BIT31_BODY();                                                     \
  }

#define JTAG_WRITE_DATA31()                                                     \
  for (n = 31U; n; n--) {                                                       \
    JTAG_WRITE_BIT31_BODY();                                                    \
  }


// JTAG Set IR
//   ir:     IR value
//   return: none
#define JTAG_IR_Function(speed) /**/                                            \
static void JTAG_IR_##speed (uint32_t ir) {                                     \
  uint32_t n;                                                                   \
  SWJ_INIT();                                                                   \
                                                                                \
  PIN_TMS_SET();                                                                \
  JTAG_CYCLE_TCK();                         /* Select-DR-Scan */                \
  JTAG_CYCLE_TCK();                         /* Select-IR-Scan */                \
  PIN_TMS_CLR();                                                                \
  JTAG_CYCLE_TCK();                         /* Capture-IR */                    \
  JTAG_CYCLE_TCK();                         /* Shift-IR */                      \
                                                                                \
  SWJ_TDI_OUT(1U);                                                              \
  for (n = DAP_Data.jtag_dev.ir_before[DAP_Data.jtag_dev.index]; n; n--) {      \
    JTAG_CYCLE_TCK();                       /* Bypass before data */            \
  }                                                                             \
  for (n = DAP_Data.jtag_dev.ir_length[DAP_Data.jtag_dev.index] - 1U; n; n--) { \
    JTAG_CYCLE_TDI(ir);                     /* Set IR bits (except last) */     \
    ir >>= 1;                                                                   \
  }                                                                             \
  n = DAP_Data.jtag_dev.ir_after[DAP_Data.jtag_dev.index];                      \
  if (n) {                                                                      \
    JTAG_CYCLE_TDI(ir);                     /* Set last IR bit */               \
    SWJ_TDI_OUT(1U);                                                            \
    for (--n; n; n--) {                                                         \
      JTAG_CYCLE_TCK();                     /* Bypass after data */             \
    }                                                                           \
    PIN_TMS_SET();                                                              \
    JTAG_CYCLE_TCK();                       /* Bypass & Exit1-IR */             \
  } else {                                                                      \
    PIN_TMS_SET();                                                              \
    JTAG_CYCLE_TDI(ir);                     /* Set last IR bit & Exit1-IR */    \
  }                                                                             \
                                                                                \
  JTAG_CYCLE_TCK();                         /* Update-IR */                     \
  PIN_TMS_CLR();                                                                \
  JTAG_CYCLE_TCK();                         /* Idle */                          \
  SWJ_TDI_OUT(1U);                                                              \
}


// JTAG Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
#define JTAG_TransferFunction(speed)        /**/                                \
static uint8_t JTAG_Transfer##speed (uint32_t request, uint32_t *data) {        \
  uint32_t ack;                                                                 \
  uint32_t bit;                                                                 \
  uint32_t val;                                                                 \
  uint32_t n;                                                                   \
  SWJ_INIT();                                                                   \
                                                                                \
  PIN_TMS_SET();                                                                \
  JTAG_CYCLE_TCK();                         /* Select-DR-Scan */                \
  PIN_TMS_CLR();                                                                \
  JTAG_CYCLE_TCK();                         /* Capture-DR */                    \
  JTAG_CYCLE_TCK();                         /* Shift-DR */                      \
                                                                                \
  for (n = DAP_Data.jtag_dev.index; n; n--) {                                   \
    JTAG_CYCLE_TCK();                       /* Bypass before data */            \
  }                                                                             \
                                                                                \
  JTAG_CYCLE_TDIO(request >> 1, bit);       /* Set RnW, Get ACK.0 */            \
  ack  = bit << 1;                                                              \
  JTAG_CYCLE_TDIO(request >> 2, bit);       /* Set A2,  Get ACK.1 */            \
  ack |= bit << 0;                                                              \
  JTAG_CYCLE_TDIO(request >> 3, bit);       /* Set A3,  Get ACK.2 */            \
  ack |= bit << 2;                                                              \
                                                                                \
  if (ack != DAP_TRANSFER_OK) {                                                 \
    /* Exit on error */                                                         \
    PIN_TMS_SET();                                                              \
    JTAG_CYCLE_TCK();                       /* Exit1-DR */                      \
    goto exit;                                                                  \
  }                                                                             \
                                                                                \
  if (request & DAP_TRANSFER_RnW) {                                             \
    /* Read Transfer */                                                         \
    val = 0U;                                                                   \
    JTAG_READ_DATA31();                     /* Get D0..D30 */                   \
    n = DAP_Data.jtag_dev.count - DAP_Data.jtag_dev.index - 1U;                 \
    if (n) {                                                                    \
      JTAG_CYCLE_TDO(bit);                  /* Get D31 */                       \
      for (--n; n; n--) {                                                       \
        JTAG_CYCLE_TCK();                   /* Bypass after data */             \
      }                                                                         \
      PIN_TMS_SET();                                                            \
      JTAG_CYCLE_TCK();                     /* Bypass & Exit1-DR */             \
    } else {                                                                    \
      PIN_TMS_SET();                                                            \
      JTAG_CYCLE_TDO(bit);                  /* Get D31 & Exit1-DR */            \
    }                                                                           \
    val |= bit << 31;                                                           \
    if (data) { *data = val; }                                                  \
  } else {                                                                      \
    /* Write Transfer */                                                        \
    val = *data;                                                                \
    JTAG_WRITE_DATA31();                    /* Set D0..D30 */                   \
    n = DAP_Data.jtag_dev.count - DAP_Data.jtag_dev.index - 1U;                 \
    if (n) {                                                                    \
      JTAG_CYCLE_TDI(val);                  /* Set D31 */                       \
      for (--n; n; n--) {                                                       \
        JTAG_CYCLE_TCK();                   /* Bypass after data */             \
      }                                                                         \
      PIN_TMS_SET();                                                            \
      JTAG_CYCLE_TCK();                     /* Bypass & Exit1-DR */             \
    } else {                                                                    \
      PIN_TMS_SET();                                                            \
      JTAG_CYCLE_TDI(val);                  /* Set D31 & Exit1-DR */            \
    }                                                                           \
  }                                                                             \
                                                                                \
exit:                                                                           \
  JTAG_CYCLE_TCK();                         /* Update-DR */                     \
  PIN_TMS_CLR();                                                                \
  JTAG_CYCLE_TCK();                         /* Idle */                          \
  SWJ_TDI_OUT(1U);                                                              \
                                                                                \
  /* Capture Timestamp */                                                       \
  if (request & DAP_TRANSFER_TIMESTAMP) {                                       \
    DAP_Data.timestamp = TIMESTAMP_GET();                                       \
  }                                                                             \
                                                                                \
  /* Idle cycles */                                                             \
  n = DAP_Data.transfer.idle_cycles;                                            \
  while (n--) {                                                                 \
    JTAG_CYCLE_TCK();                       /* Idle */                          \
  }                                                                             \
                                                                                \
  return ((uint8_t)ack);                                                        \
}


JTAG_IR_Function(Timed)
JTAG_TransferFunction(Timed)


//**************************************************************************************************
// FAST bands (always in RAM)

#pragma arm section code = "ram_func"

#undef  SWJ_INIT
#define SWJ_INIT()

#if (SWJ_FAST_UNROLL != 0)
// Every data bit runs straight-line code (no loop branch)
#undef  JTAG_READ_DATA31
#undef  JTAG_WRITE_DATA31
#define JTAG_READ_DATA31()  do { SWJ_REP31(JTAG_READ_BIT31_BODY()); } while (0)
#define JTAG_WRITE_DATA31() do { SWJ_REP31(JTAG_WRITE_BIT31_BODY()); } while (0)
#endif

#undef  PIN_DELAY
#undef  PIN_DELAY_RD
#undef  PIN_DELAY_RD_HI
#undef  PIN_DELAY_WR_HI
#define PIN_DELAY()         SWJ_FAST_DELAY(JTAG_FAST1_NOPS)
#define PIN_DELAY_RD()      SWJ_FAST_DELAY(JTAG_FAST1_RD_NOPS)
#define PIN_DELAY_RD_HI()   SWJ_FAST_DELAY(JTAG_FAST1_RD_HI_NOPS)
#define PIN_DELAY_WR_HI()   SWJ_FAST_DELAY(JTAG_FAST1_WR_HI_NOPS)
JTAG_IR_Function(Fast1)
JTAG_TransferFunction(Fast1)

#if (SWJ_FAST2_ENABLE != 0)
#undef  PIN_DELAY
#undef  PIN_DELAY_RD
#undef  PIN_DELAY_RD_HI
#undef  PIN_DELAY_WR_HI
#define PIN_DELAY()         SWJ_FAST_DELAY(JTAG_FAST2_NOPS)
#define PIN_DELAY_RD()      SWJ_FAST_DELAY(JTAG_FAST2_RD_NOPS)
#define PIN_DELAY_RD_HI()   SWJ_FAST_DELAY(JTAG_FAST2_RD_HI_NOPS)
#define PIN_DELAY_WR_HI()   SWJ_FAST_DELAY(JTAG_FAST2_WR_HI_NOPS)
JTAG_IR_Function(Fast2)
JTAG_TransferFunction(Fast2)
#endif


// JTAG Set IR
//   ir:     IR value
//   return: none
void JTAG_IR (uint32_t ir) {
  SWJ_CheckClock();
  switch (jtag_band) {
    case SWJ_BAND_FAST1: JTAG_IR_Fast1(ir); break;
#if (SWJ_FAST2_ENABLE != 0)
    case SWJ_BAND_FAST2: JTAG_IR_Fast2(ir); break;
#endif
    default:             JTAG_IR_Timed(ir); break;
  }
}


// JTAG Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
uint8_t JTAG_Transfer (uint32_t request, uint32_t *data) {
  SWJ_CheckClock();
  switch (jtag_band) {
    case SWJ_BAND_FAST1: return JTAG_TransferFast1(request, data);
#if (SWJ_FAST2_ENABLE != 0)
    case SWJ_BAND_FAST2: return JTAG_TransferFast2(request, data);
#endif
    default:             return JTAG_TransferTimed(request, data);
  }
}

#pragma arm section code

#pragma pop

#endif  /* __CC_ARM && DAP_JTAG */
