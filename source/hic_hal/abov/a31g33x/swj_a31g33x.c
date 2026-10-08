/**
 * @file    swj_a31g33x.c
 * @brief   A31G33x SWD/JTAG clock band selection and timer (see swj_a31g33x.h)
 *
 * DAPLink Interface Firmware
 * SPDX-License-Identifier: Apache-2.0
 */

#include "swj_a31g33x.h"

uint32_t swj_clock = 0U;
uint32_t swj_half  = SWJ_MIN_HALF_CYCLES;
uint8_t  swd_band  = SWJ_BAND_TIMED;
uint8_t  jtag_band = SWJ_BAND_TIMED;

/// Start the free-running 32-bit timer used by the TIMED band (called from DAP_SETUP)
void SWJ_TimerInit(void)
{
    if (SWJ_TIMER->CR & TIMER20_CR_T2nEN_Msk) {
        return;                                 /* already running (DAP_Setup called again) */
    }

    SCUCG->PPCLKSR  |= SWJ_TIMER_CLKSR_Msk;     /* TIMER clock = PCLK */
    SCUCG->PPCLKEN1 |= SWJ_TIMER_CLKE_Msk;      /* TIMER clock enable */

    SWJ_TIMER->CR    = 0U;                      /* stop, periodic (match) mode, internal clock, no IRQ */
    SWJ_TIMER->PREDR = 0U;                      /* fT = PCLK / (0 + 1) */
    SWJ_TIMER->ADR   = 0xFFFFFFFFU;             /* match at max -> 32-bit wrap */
    SWJ_TIMER->CR    = TIMER20_CR_T2nEN_Msk | TIMER20_CR_T2nCLR_Msk;
}

static uint8_t SWJ_Band(uint32_t clock, uint32_t fast1, uint32_t fast2)
{
    if (clock >= fast1) {
        return SWJ_BAND_FAST1;
    }
#if (SWJ_FAST2_ENABLE != 0)
    if (clock >= fast2) {
        return SWJ_BAND_FAST2;
    }
#endif
    return SWJ_BAND_TIMED;
}

/// Recompute the bands and the TIMED half period from DAP_Data.nominal_clock
void SWJ_ClockUpdate(void)
{
    uint32_t clock = DAP_Data.nominal_clock;
    uint32_t half;

    if (clock == 0U) {
        clock = DAP_DEFAULT_SWJ_CLOCK;
    }

    half = (SWJ_TIMER_CLOCK + (2U * clock - 1U)) / (2U * clock);
    if (half < SWJ_MIN_HALF_CYCLES) {
        half = SWJ_MIN_HALF_CYCLES;
    }

    swj_half  = half;
    swd_band  = SWJ_Band(clock, SWD_FAST1_CLOCK,  SWD_FAST2_CLOCK);
    jtag_band = SWJ_Band(clock, JTAG_FAST1_CLOCK, JTAG_FAST2_CLOCK);
    swj_clock = DAP_Data.nominal_clock;
}
