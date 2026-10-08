/**
 * @file    swj_a31g33x.h
 * @brief   A31G33x SWD/JTAG clock generation (HIC specific, used by SW_DP_ram.c and JTAG_DP_ram.c)
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 *
 * The common cmsis-dap/SW_DP.c and JTAG_DP.c are left untouched. This HIC replaces them:
 *  - SWD : strong SWJ_Sequence/SWD_Sequence/SWD_Transfer override the __WEAK ones in SW_DP.c
 *  - JTAG: $Sub$$JTAG_xxx (armlink patching) replace the JTAG_xxx functions in JTAG_DP.c
 *
 * Clock bands are selected from DAP_Data.nominal_clock (set by DAP_SWJ_Clock in DAP.c), so
 * DAP_Data.fast_clock / clock_delay of the common DAP.c are not used here.
 *   clock >= xxx_FAST1_CLOCK : FAST1, fixed NOPs per half period (max speed)
 *   clock >= xxx_FAST2_CLOCK : FAST2, fixed NOPs per half period
 *   otherwise                : TIMED, half period timed with a free-running 32-bit TIMER2n
 *                              (Cortex-M0+ has no DWT->CYCCNT)
 * Sequences, JTAG_ReadIDCode and JTAG_WriteAbort are always TIMED.
 *
 * Code runs from RAM ("ram_func" in RW_IRAM1 of daplink.sct): flash needs 2-wait at 48MHz.
 * Any change of these values or of the code must be re-measured with a scope.
 */

#ifndef SWJ_A31G33X_H
#define SWJ_A31G33X_H

#include "DAP_config.h"
#include "DAP.h"

//**************************************************************************************************
// Configuration (each value can be overridden in DAP_config.h)

/// Free-running timer for the TIMED band (TIMER20, 32-bit, PCLK, no prescaler)
#ifndef SWJ_TIMER
#define SWJ_TIMER               TIMER20
#define SWJ_TIMER_CLKE_Msk      SCUCG_PPCLKEN1_T20CLKE_Msk     ///< SCUCG->PPCLKEN1
#define SWJ_TIMER_CLKSR_Msk     SCUCG_PPCLKSR_T20CLK_Msk       ///< SCUCG->PPCLKSR (1 = PCLK)
#endif
#ifndef SWJ_TIMER_CLOCK
#define SWJ_TIMER_CLOCK         SystemCoreClock ///< Timer tick in Hz (PCLK = HCLK = 48MHz)
#endif

/// Every TIMED half period is at least this many timer ticks
#ifndef SWJ_MIN_HALF_CYCLES
#define SWJ_MIN_HALF_CYCLES     8U
#endif

/// 1 = FAST2 band available (costs RAM), 0 = FAST1 + TIMED only
/// RAM (ram_func, armcc -O3 -Otime, FAST1 NOPS 3 / WR_HI 0, 2026-10-08):
///   SWJ_FAST2_ENABLE=0 (default)         : SWD 3.3KB + JTAG 4.3KB =  7.6KB
///   SWJ_FAST2_ENABLE=1                   : SWD 6.7KB + JTAG 6.9KB = 13.6KB
///   SWJ_FAST_UNROLL=0                    : SWD 2.1KB + JTAG 2.9KB =  5.0KB
/// Build 2026-10-08: RW 1004 + ZI 9496 = 10.3KB (+ ram_func code) of RW_IRAM1 0x5F00 (23.75KB)
/// -> FAST2 with unroll does not fit.
#ifndef SWJ_FAST2_ENABLE
#define SWJ_FAST2_ENABLE        0
#endif

/// 1 = 32-bit data phase of the FAST transfers unrolled (uniform SWCLK/TCK, costs RAM)
#ifndef SWJ_FAST_UNROLL
#define SWJ_FAST_UNROLL         1
#endif

/// 1 = TIMED functions in RAM, 0 = in flash (saves RAM, lower usable TIMED clock)
#ifndef SWJ_TIMED_IN_RAM
#define SWJ_TIMED_IN_RAM        1
#endif

/// SWD FAST bands: NOPs per half period (0..16). RD = read bit low half (before SWDIO read),
/// RD_HI = read bit high half (after it).
///   Measured on SWCLK (2026-10-08, Keil 10MHz): WDATA 292ns (5+9 cycles) = 3.42MHz
///   ABOV targets: SWCLK <= 5MHz -> keep every half period >= 5 cycles (104ns @ 48MHz):
///   NOPS 3 -> CLR, 3 NOPs, SET = ~5 cycles; RD 1 -> CLR, NOP, INDR read, SET = ~5 cycles.
///   Disassembly (FAST1): RDATA 12 cycles (~4.0MHz), WDATA 14 cycles (~3.4MHz),
///   parity computed once per word (SWJ_Parity32) instead of per bit.
#ifndef SWD_FAST1_CLOCK
#define SWD_FAST1_CLOCK         4000000U
#endif
#ifndef SWD_FAST1_NOPS
#define SWD_FAST1_NOPS          3U
#endif
#ifndef SWD_FAST1_RD_NOPS
#define SWD_FAST1_RD_NOPS       1U
#endif
#ifndef SWD_FAST1_RD_HI_NOPS
#define SWD_FAST1_RD_HI_NOPS    0U
#endif
/// WR_HI = write bit high half: the next bit preparation (SWDIO write, shift, parity) already
/// runs there, so it needs fewer NOPs than a clock-only cycle (turnaround, idle).
#ifndef SWD_FAST1_WR_HI_NOPS
#define SWD_FAST1_WR_HI_NOPS    0U
#endif
#ifndef SWD_FAST2_CLOCK
#define SWD_FAST2_CLOCK         2000000U
#endif
#ifndef SWD_FAST2_NOPS
#define SWD_FAST2_NOPS          4U
#endif
#ifndef SWD_FAST2_RD_NOPS
#define SWD_FAST2_RD_NOPS       4U
#endif
#ifndef SWD_FAST2_RD_HI_NOPS
#define SWD_FAST2_RD_HI_NOPS    4U
#endif
#ifndef SWD_FAST2_WR_HI_NOPS
#define SWD_FAST2_WR_HI_NOPS    2U
#endif

/// JTAG FAST bands: target changes TDO on the TCK falling edge, so RD (low half before the
/// TDO read) must cover TDO output delay + cable + GPIO input sync.
///   Same 5 cycles (104ns) minimum half period as SWD (NOPS 3). Measured on TCK (2026-10-08):
///   - unrolled, 0 NOPs        : pulses down to 2+2 cycles (82ns, 12MHz) -> too fast
///   - NOPS 3 both halves      : TDI data 354ns (12+5 cycles) = 2.82MHz, TCK-only 228ns
///   - NOPS 3, WR_HI 0 (now)   : TDI data 292ns (9+5 cycles) = 3.42MHz (Keil 10MHz),
///                               IR/loop bits 356ns (12+5) = 2.81MHz (IAR)
///   - same, 2nd capture       : TDO data 250ns (5+7 cycles) = 4.0MHz (IAR),
///                               TDI data 292ns (5+9) = 3.42MHz, post-data bit 416ns (Keil)
///   Disassembly (now): TDI data 14, TDO data 12 (TDO sampled ~4 cycles after TCK fall),
///   TCK-only 10, bypass/idle loop 13, IR loop ~17 cycles.
#ifndef JTAG_FAST1_CLOCK
#define JTAG_FAST1_CLOCK        3400000U    ///< ~ measured data bit rate
#endif
#ifndef JTAG_FAST1_NOPS
#define JTAG_FAST1_NOPS         3U
#endif
#ifndef JTAG_FAST1_RD_NOPS
#define JTAG_FAST1_RD_NOPS      3U
#endif
#ifndef JTAG_FAST1_RD_HI_NOPS
#define JTAG_FAST1_RD_HI_NOPS   0U
#endif
#ifndef JTAG_FAST1_WR_HI_NOPS
#define JTAG_FAST1_WR_HI_NOPS   0U          ///< TDI write bit high half (next TDI write follows)
#endif
#ifndef JTAG_FAST2_CLOCK
#define JTAG_FAST2_CLOCK        1800000U
#endif
#ifndef JTAG_FAST2_NOPS
#define JTAG_FAST2_NOPS         3U
#endif
#ifndef JTAG_FAST2_RD_NOPS
#define JTAG_FAST2_RD_NOPS      5U
#endif
#ifndef JTAG_FAST2_RD_HI_NOPS
#define JTAG_FAST2_RD_HI_NOPS   2U
#endif
#ifndef JTAG_FAST2_WR_HI_NOPS
#define JTAG_FAST2_WR_HI_NOPS   1U
#endif

//**************************************************************************************************
// Clock band state (swj_a31g33x.c)

#define SWJ_BAND_TIMED          0U
#define SWJ_BAND_FAST1          1U
#define SWJ_BAND_FAST2          2U

extern uint32_t swj_clock;      ///< nominal clock the state below was computed for
extern uint32_t swj_half;       ///< TIMED half period in timer ticks
extern uint8_t  swd_band;       ///< SWJ_BAND_xxx for SWD
extern uint8_t  jtag_band;      ///< SWJ_BAND_xxx for JTAG

extern void SWJ_TimerInit(void);
extern void SWJ_ClockUpdate(void);

/// Recompute bands only when the debugger changed the SWJ clock
__STATIC_FORCEINLINE void SWJ_CheckClock(void)
{
    if (DAP_Data.nominal_clock != swj_clock) {
        SWJ_ClockUpdate();
    }
}

//**************************************************************************************************
// Pin helpers (register access only, no HAL call: they run from RAM)

/// SWDIO write without branch: BSR (+0x1C) for 1, BCR (+0x20) for 0 -> same timing for 0/1
__STATIC_FORCEINLINE void SWJ_SWDIO_OUT(uint32_t bit)
{
    (&PB->BSR)[(bit & 1U) ^ 1U] = 1UL << SWDIO_OUT_PIN;
}

/// TDI write without branch
__STATIC_FORCEINLINE void SWJ_TDI_OUT(uint32_t bit)
{
    (&PA->BSR)[(bit & 1U) ^ 1U] = 1UL << JTAG_TDI_PIN;
}

/// Parity of a 32-bit word (XOR folding). Used once per data phase instead of adding every
/// bit: on Cortex-M0+ the per-bit parity add spills to r12 and costs ~3 cycles per bit.
__STATIC_FORCEINLINE uint32_t SWJ_Parity32(uint32_t v)
{
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1U;
}

/// SWDIO to output push-pull, driving high (same result as PIN_SWDIO_OUT_ENABLE)
__STATIC_FORCEINLINE void SWJ_SWDIO_OUT_ENABLE(void)
{
    PB->BSR = 1UL << SWDIO_OUT_PIN;
    PB->MOD = (PB->MOD & ~(3UL << (SWDIO_OUT_PIN * 2U))) | (1UL << (SWDIO_OUT_PIN * 2U));
}

/// SWDIO to input (same result as PIN_SWDIO_OUT_DISABLE)
__STATIC_FORCEINLINE void SWJ_SWDIO_OUT_DISABLE(void)
{
    PB->MOD &= ~(3UL << (SWDIO_OUT_PIN * 2U));
}

//**************************************************************************************************
// Half period delays

/// TIMED: waits until swj_h ticks have passed since the previous edge. If the code was late
/// (SWDIO direction switch, interrupt), timing restarts from now so the next half is never
/// shortened. The start reference is pushed SWJ_MIN_HALF_CYCLES ahead so the first half covers
/// the code executed before the first edge.
#define SWJ_TIMER_CNT()         (SWJ_TIMER->CNT)

#define SWJ_TIMED_INIT()                                                       \
  uint32_t swj_h = swj_half;                                                   \
  uint32_t swj_t = SWJ_TIMER_CNT() + SWJ_MIN_HALF_CYCLES

/// The wait is a function (not inline) to keep the TIMED code small: the call overhead does not
/// change the period, it only lowers the highest clock the TIMED band can reach (~1.5MHz).
/// Each .c defines it with SWJ_TIMED_WAIT_FUNCTION() inside its TIMED code section.
#define SWJ_TIMED_WAIT_FUNCTION()                                              \
static __attribute__((noinline)) uint32_t swj_timed_wait(uint32_t t, uint32_t h) { \
  uint32_t now;                                                                \
  do {                                                                         \
    now = SWJ_TIMER_CNT();                                                     \
  } while ((int32_t)(now - t) < (int32_t)h);                                   \
  t += h;                                                                      \
  if ((int32_t)(now - t) > 0) {                                                \
    t = now;                                                                   \
  }                                                                            \
  return t;                                                                    \
}

#define SWJ_TIMED_DELAY()       (swj_t = swj_timed_wait(swj_t, swj_h))

/// FAST: fixed NOPs (n is a constant, the unused ones are removed by the compiler)
#define SWJ_NOP(n, i)           if ((n) > (i)) { __NOP(); }
#define SWJ_FAST_DELAY(n) do {                                                 \
    SWJ_NOP(n, 0)  SWJ_NOP(n, 1)  SWJ_NOP(n, 2)  SWJ_NOP(n, 3)                 \
    SWJ_NOP(n, 4)  SWJ_NOP(n, 5)  SWJ_NOP(n, 6)  SWJ_NOP(n, 7)                 \
    SWJ_NOP(n, 8)  SWJ_NOP(n, 9)  SWJ_NOP(n, 10) SWJ_NOP(n, 11)                \
    SWJ_NOP(n, 12) SWJ_NOP(n, 13) SWJ_NOP(n, 14) SWJ_NOP(n, 15)                \
  } while (0)

/// Straight-line repetition for the unrolled data phase
#define SWJ_REP4(x)             x; x; x; x
#define SWJ_REP16(x)            SWJ_REP4(x); SWJ_REP4(x); SWJ_REP4(x); SWJ_REP4(x)
#define SWJ_REP31(x)            SWJ_REP16(x); SWJ_REP4(x); SWJ_REP4(x); SWJ_REP4(x); x; x; x
#define SWJ_REP32(x)            SWJ_REP16(x); SWJ_REP16(x)

#endif /* SWJ_A31G33X_H */
