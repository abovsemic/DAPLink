/**
 *******************************************************************************
 * @file        startup_a31xxxx.c
 * @author      ABOV R&D Division
 * @brief       CMSIS-Core(M) Device Startup File for a Cortex-M0+ Device
 *
 * Copyright 2022 ABOV Semiconductor Co.,Ltd. All rights reserved.
 *
 * This file is licensed under terms that are found in the LICENSE file
 * located at Document directory.
 * If this file is delivered or shared without applicable license terms,
 * the terms of the BSD-3-Clause license shall be applied.
 * Reference: https://opensource.org/licenses/BSD-3-Clause
 ******************************************************************************/


#include "abov_config.h"
#include "daplink_addr.h"
#include <rt_misc.h>

/*----------------------------------------------------------------------------
  Internal Macros
 *----------------------------------------------------------------------------*/
#if (CONFIG_EMUL_JTAG_CONNECTION == 1)
#define PRV_CHIPSET_REG_DHCSR  0xE000EDF0
#endif

#pragma import(__use_no_semihosting_swi)
/*----------------------------------------------------------------------------
  External References
 *----------------------------------------------------------------------------*/
#define INITIAL_SP_VALUE  (DAPLINK_RAM_APP_START + DAPLINK_RAM_APP_SIZE)

#if (CONFIG_EMUL_JTAG_CONNECTION == 1)
int8_t g_bIsARMisDebugMode = 0;
int8_t g_bIsARMisDebugConnectionChecked = 0;
#endif

extern __NO_RETURN void __PROGRAM_START(void);

/*----------------------------------------------------------------------------
  Internal References
 *----------------------------------------------------------------------------*/
__NO_RETURN void Reset_Handler  (void);
            void Default_Handler(void);

/*----------------------------------------------------------------------------
  Exception / Interrupt Handler
 *----------------------------------------------------------------------------*/
/* Exceptions */
void NMI_Handler            (void) __attribute__ ((weak, alias("Default_Handler")));
void HardFault_Handler      (void) __attribute__ ((weak));
void SVC_Handler            (void) __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler         (void) __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler        (void) __attribute__ ((weak, alias("Default_Handler")));

void Interrupt0_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt1_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt2_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt3_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt4_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt5_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt6_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt7_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt8_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt9_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));

void Interrupt10_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt11_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt12_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt13_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt14_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt15_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt16_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt17_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt18_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt19_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));

void Interrupt20_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt21_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt22_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt23_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt24_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt25_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt26_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt27_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt28_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt29_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));

void Interrupt30_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));
void Interrupt31_Handler     (void) __attribute__ ((weak, alias("Default_Handler")));

/*----------------------------------------------------------------------------
  Exception / Interrupt Vector table
 *----------------------------------------------------------------------------*/

extern const VECTOR_TABLE_Type __VECTOR_TABLE[48];
       const VECTOR_TABLE_Type __VECTOR_TABLE[48] __VECTOR_TABLE_ATTRIBUTE =
{
//  (VECTOR_TABLE_Type)(&__INITIAL_SP),       /*     Initial Stack Pointer */
  INITIAL_SP_VALUE,       /*     Initial Stack Pointer */
  Reset_Handler,                            /*     Reset Handler */
  NMI_Handler,                              /* -14 NMI Handler */
  HardFault_Handler,                        /* -13 Hard Fault Handler */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  SVC_Handler,                              /*  -5 SVCall Handler */
  0,                                        /*     Reserved */
  0,                                        /*     Reserved */
  PendSV_Handler,                           /*  -2 PendSV Handler */
  SysTick_Handler,                          /*  -1 SysTick Handler */

  /* Interrupts */
  Interrupt0_Handler,                       /*   0 Interrupt 0 */
  Interrupt1_Handler,                       /*   1 Interrupt 1 */
  Interrupt2_Handler,                       /*   2 Interrupt 2 */
  Interrupt3_Handler,                       /*   3 Interrupt 3 */
  Interrupt4_Handler,                       /*   4 Interrupt 4 */
  Interrupt5_Handler,                       /*   5 Interrupt 5 */
  Interrupt6_Handler,                       /*   6 Interrupt 6 */
  Interrupt7_Handler,                       /*   7 Interrupt 7 */
  Interrupt8_Handler,                       /*   8 Interrupt 8 */
  Interrupt9_Handler,                       /*   9 Interrupt 9 */
  Interrupt10_Handler,                      /*   10 Interrupt 10 */
  Interrupt11_Handler,                      /*   11 Interrupt 11 */
  Interrupt12_Handler,                      /*   12 Interrupt 12 */
  Interrupt13_Handler,                      /*   13 Interrupt 13 */
  Interrupt14_Handler,                      /*   14 Interrupt 14 */
  Interrupt15_Handler,                      /*   15 Interrupt 15 */
  Interrupt16_Handler,                      /*   16 Interrupt 16 */
  Interrupt17_Handler,                      /*   17 Interrupt 17 */
  Interrupt18_Handler,                      /*   18 Interrupt 18 */
  Interrupt19_Handler,                      /*   19 Interrupt 19 */
  Interrupt20_Handler,                      /*   20 Interrupt 20 */
  Interrupt21_Handler,                      /*   21 Interrupt 21 */
  Interrupt22_Handler,                      /*   22 Interrupt 22 */
  Interrupt23_Handler,                      /*   23 Interrupt 23 */
  Interrupt24_Handler,                      /*   24 Interrupt 24 */
  Interrupt25_Handler,                      /*   25 Interrupt 25 */
  Interrupt26_Handler,                      /*   26 Interrupt 26 */
  Interrupt27_Handler,                      /*   27 Interrupt 27 */
  Interrupt28_Handler,                      /*   28 Interrupt 28 */
  Interrupt29_Handler,                      /*   29 Interrupt 29 */
  Interrupt30_Handler,                      /*   30 Interrupt 30 */
  Interrupt31_Handler                       /*   31 Interrupt 31 */

};

__value_in_regs struct __initial_stackheap __user_initial_stackheap(
    unsigned int r0, unsigned int r1, unsigned int r2, unsigned int r3) 
{
    struct __initial_stackheap config;
    
    config.heap_base   = DAPLINK_RAM_APP_START; 
    config.heap_limit  = DAPLINK_RAM_APP_START + 0x0400; 
    
    config.stack_base  = INITIAL_SP_VALUE;                 
    config.stack_limit = INITIAL_SP_VALUE - 0x0800;        
    
    return config;
}


/*----------------------------------------------------------------------------
  Reset Handler called on controller reset
 *----------------------------------------------------------------------------*/
__NO_RETURN void Reset_Handler(void)
{
  SystemInit();                             /* CMSIS System Initialization */
  __PROGRAM_START();                        /* Enter PreMain (C library entry point) */
}


#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif

/*----------------------------------------------------------------------------
  Hard Fault Handler
 *----------------------------------------------------------------------------*/
void HardFault_Handler(void)
{
#if (CONFIG_EMUL_JTAG_CONNECTION == 1)
    int* pn32Reg = (int*)PRV_CHIPSET_REG_DHCSR;

    /* Get Debugger connection */
    if (g_bIsARMisDebugConnectionChecked)
    {
       if (*(pn32Reg) & 0x01)
       {
          g_bIsARMisDebugMode = 1;
       }

       g_bIsARMisDebugConnectionChecked = 0;
       return;
    }
#endif

  while(1);
}

/*----------------------------------------------------------------------------
  Default Handler for Exceptions / Interrupts
 *----------------------------------------------------------------------------*/
void Default_Handler(void)
{
  while(1);
}

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
  #pragma clang diagnostic pop
#endif

/*----------------------------------------------------------------------------
  Private API to detect debugger connection
 *----------------------------------------------------------------------------*/
#define PRV_CHIPSET_INVALID_ADD   0x99999999UL
#define PRV_CHIPSET_INVALID_DATA  0x50

#if (CONFIG_EMUL_JTAG_CONNECTION == 1)
void PRV_CHIPSET_SetDebuggerConnectionStatus (void)
{
    uint8_t *un8pAddr = (uint8_t *)PRV_CHIPSET_INVALID_ADD;

    g_bIsARMisDebugConnectionChecked = 1;

    /* Fire Hard-Fault */
    *un8pAddr = PRV_CHIPSET_INVALID_DATA;
    return;
}

int8_t PRV_CHIPSET_GetDebuggerConnectionStatus (void)
{
    return g_bIsARMisDebugMode;
}
#endif

