/**
 *******************************************************************************
 * @file        abov_config.h
 * @author      ABOV R&D Division
 * @brief       Top level configuration file
 *
 * Copyright 2022 ABOV Semiconductor Co.,Ltd. All rights reserved.
 *
 * This file is licensed under terms that are found in the LICENSE file
 * located at Document directory.
 * If this file is delivered or shared without applicable license terms,
 * the terms of the BSD-3-Clause license shall be applied.
 * Reference: https://opensource.org/licenses/BSD-3-Clause
 ******************************************************************************/

/** @addtogroup VENDOR ABOV Semiconductor Co., Ltd.
  * @{
*/


/* @addtogroup Simple Application Config
  * @{
*/

#ifndef ABOV_SIMPLEAPP_CONFIG_H
#define ABOV_SIMPLEAPP_CONFIG_H

/*
//-------- <<< Use Configuration Wizard in Context Menu >>> -------------------
*/

#define PCU_GPIO_ISR                  0
// <h> PCU (Port Control Unit)
//  <e> PCU Application
#define CONFIG_APP_PCU        0
//  <o> PCU_APP: Port Control Unit
//     <0=> GPIO Interrupt
//  <i> Default: 0
#define PCU_APP 0
//  </e>
// </h> End of PCU

#define FMC_APP_CODE_FLASH            0
#define FMC_APP_DATA_FLASH            1
// <h> FMC (Embedded Flash Memory)
//  <e> FMC Application
#define CONFIG_APP_FMC        0
//  <o> FMC_APP: Target Flash
//     <0=> Code Flash
//     <1=> Data Flash
//  <i> Default: 0
#define FMC_APP 0
//  </e>
// </h> End of FMC

#define CRC_APP_POLY_32               0
#define CRC_APP_POLY_16_USB           1
#define CRC_APP_POLY_16_CCITT         2
#define CRC_APP_POLY_8_CCITT          3
// <h> CRC (Cyclic Redundancy Check Calculation Module)
//  <e> CRC Application
#define CONFIG_APP_CRC        0
//  <o> CRC_APP: Polynomial
//     <0=> CRC-32
//     <1=> CRC-16 USB
//     <2=> CRC-16 CCITT
//     <3=> CRC-8 CCITT
//  <i> Default: 0
#define CRC_APP 0
//  </e>
// </h> End of CRC

#define TIMER1_APP_MODE_PERIODIC      0
#define TIMER1_APP_MODE_ONESHOT       1
#define TIMER1_APP_MODE_PWM           2
#define TIMER1_APP_MODE_CAPTURE       3
// <h> TIMER1 (16bit Timer)
//  <e> TIMER1 Application
#define CONFIG_APP_TIMER1     0
//  <o> TIMER1_APP: Operating mode
//     <0=> Periodic mode
//     <1=> One-shot mode
//     <2=> PWM mode
//     <3=> Capture mode
//  <i> Default: 0
#define TIMER1_APP 0
//  </e>
// </h> End of TIMER1

#define I2C_APP_MODE_RX              0
#define I2C_APP_MODE_TX              1
// <h> I2C (Inter-Integrated Circuit)
//  <e> I2C Application
#define CONFIG_APP_I2C      0
//  <o> I2C_APP: Operating mode
//     <0=> Receive mode
//     <1=> Transmit mode
//  <i> Default: 0
#define I2C_APP 0
//  </e>
// </h> End of I2C

#define UART_APP_MODE_RX              0
#define UART_APP_MODE_TX              1
// <h> UART (Universal Asynchronous Receiver/Transmitter)
//  <e> UART Application
#define CONFIG_APP_UART      0
//  <o> UART_APP: Operating mode
//     <0=> Receive mode
//     <1=> Transmit mode
//  <i> Default: 0
#define UART_APP 0
//  </e>
// </h> End of UART

#define USART_APP_MODE_UART_RX        0
#define USART_APP_MODE_UART_TX        1
#define USART_APP_MODE_USRT_RX        2
#define USART_APP_MODE_USRT_TX        3
#define USART_APP_MODE_SPI_RX         4
#define USART_APP_MODE_SPI_TX         5
// <h> USART (Universal Synchronous and Asynchronous Receiver/Transmitter)
//  <e> USART Application
#define CONFIG_APP_USART      0
//  <o> USART_APP: Operating mode
//     <0=> UART Rx mode
//     <1=> UART Tx mode
//     <2=> USRT Rx mode
//     <3=> USRT Tx mode
//     <4=> SPI Rx mode
//     <5=> SPI Tx mode
//  <i> Default: 0
#define USART_APP 0
//  </e>
// </h> End of USART

#define ADC_APP_MODE_SINGLE           0
#define ADC_APP_MODE_SEQUENTIAL       1
#define ADC_APP_MODE_MULTIPLE         2
#define ADC_APP_MODE_BURST            3
#define ADC_APP_MODE_COMPARISON       4
// <h> ADC (Analog-to-Digital Converter)
//  <e> ADC Application
#define CONFIG_APP_ADC        0
//  <o> ADC_APP: Conversion mode
//     <0=> Single mode
//     <1=> Sequential mode
//     <2=> Multiple mode
//     <3=> Burst mode
//     <4=> Comparison mode
//  <i> Default: 0
#define ADC_APP 0
//  </e>
// </h> End of ADC

#define WDT_APP_MODE_UNDERFLOW        0
#define WDT_APP_MODE_RESET            1
// <h> WDT (Watchdog Timer)
//  <e> WDT Application
#define CONFIG_APP_WDT        0
//  <o> WDT_APP: Operating mode
//     <0=> Underflow mode
//     <1=> Reset mode
//  <i> Default: 0
#define WDT_APP 0
//  </e>
// </h> End of WDT

#define SCU_APP_CLKOUTPUT             0
#define SCU_APP_LVD_INDICATOR         1
#define SCU_APP_LVD_RESET             2
#define SCU_APP_PWR_DEEPSLEEP         3
// <h> SCU (System Control Unit)
//  <e> SCU Application
#define CONFIG_APP_SCU        0
//  <o> SCU_APP: Operating mode
//     <0=> Clock output
//     <1=> Low-Voltage Indicator
//     <2=> Low-Voltage Reset
//  <i> Default: 0
#define SCU_APP 0
//  </e>
// </h> End of SCU

#if (CONFIG_APP_FMC == 1)
    #define CONFIG_HAL_FMC        1
#endif
#if (CONFIG_APP_CRC == 1)
    #define CONFIG_HAL_CRC        1
#endif
#if (CONFIG_APP_TIMER1 == 1)
    #define CONFIG_HAL_TIMER1     1
#endif
#if (CONFIG_APP_I2C == 1)
    #define CONFIG_HAL_I2C        1
#endif
#if (CONFIG_APP_UART == 1)
    #define CONFIG_HAL_UART       1
#endif
#if (CONFIG_APP_USART == 1)
    #define CONFIG_HAL_USART      1
#endif
#if (CONFIG_APP_ADC == 1)
    #define CONFIG_HAL_ADC        1
    #define CONFIG_HAL_TIMER1     1
#endif
#if (CONFIG_APP_WDT == 1)
    #define CONFIG_HAL_WDT        1
#endif
#if (CONFIG_APP_SCU == 1)
    #define CONFIG_HAL_SCU_LVD     1
    #define CONFIG_HAL_SCU_PWR     1
#endif

#define CONFIG_APP_SIMPLE (CONFIG_APP_PCU + CONFIG_APP_FMC + CONFIG_APP_CRC + CONFIG_APP_TIMER1 + CONFIG_APP_I2C + CONFIG_APP_UART + CONFIG_APP_USART + CONFIG_APP_ADC + CONFIG_APP_WDT + CONFIG_APP_SCU)

#if (CONFIG_APP_SIMPLE == 0) || (CONFIG_APP_SIMPLE > 1)
#error "Select one of simple application at abov_simpleapp_config.h."
#endif

#endif /* ABOV_SIMPLEAPP_CONFIG_H */
/** @} */ /* End of group Simple Application Config */

/** @} */ /* End of group VENDOR ABOV Semiconductor Co., Ltd. */
