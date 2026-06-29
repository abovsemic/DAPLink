/**
 *******************************************************************************
 * @file        abov_example_config.h
 * @author      ABOV R&D Division
 * @brief       ABOV Example Selection and Configuration.
 *
 * Copyright 2022 ABOV Semiconductor Co.,Ltd. All rights reserved.
 *
 * This file is licensed under terms that are found in the LICENSE file
 * located at Document directory.
 * If this file is delivered or shared without applicable license terms,
 * the terms of the BSD-3-Clause license shall be applied.
 * Reference: https://opensource.org/licenses/BSD-3-Clause
 ******************************************************************************/

#ifndef _EXAMPLE_CONFIG_H_
#define _EXAMPLE_CONFIG_H_

#include "abov_config.h"

#define BASE_EXAMPLE_BASE     0x00000
#define STK_EXAMPLE_BASE      0x00100
#define STKS_EXAMPLE_BASE     0x10000

#define EXAMPLE_NONE          0
#define BARE_EXAMPLE_1        (1 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_2        (2 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_3        (3 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_4        (4 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_5        (5 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_6        (6 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_7        (7 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_8        (8 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_9        (9 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_10       (10 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_11       (11 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_12       (12 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_13       (13 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_14       (14 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_15       (15 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_16       (16 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_17       (17 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_18       (18 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_19       (19 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_20       (20 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_21       (21 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_22       (22 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_23       (23 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_24       (24 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_25       (25 + BASE_EXAMPLE_BASE)
#define BARE_EXAMPLE_26       (26 + BASE_EXAMPLE_BASE)

#define STK_EXAMPLE_1         (1 + STK_EXAMPLE_BASE)
#define STK_EXAMPLE_2         (2 + STK_EXAMPLE_BASE)
#define STK_EXAMPLE_3         (3 + STK_EXAMPLE_BASE)
#define STK_EXAMPLE_4         (4 + STK_EXAMPLE_BASE)
#define STK_EXAMPLE_5         (5 + STK_EXAMPLE_BASE)

#define STKS_EXAMPLE_1        (1 + STKS_EXAMPLE_BASE)
#define STKS_EXAMPLE_2        (2 + STKS_EXAMPLE_BASE)
#define STKS_EXAMPLE_3        (3 + STKS_EXAMPLE_BASE)
#define STKS_EXAMPLE_4        (4 + STKS_EXAMPLE_BASE)
#define STKS_EXAMPLE_5        (5 + STKS_EXAMPLE_BASE)

/*
//-------- <<< Use Configuration Wizard in Context Menu >>> -------------------
*/

// <h> ABOV Example Configuration

//  <n> Customize example.
//  <n> Note. Consider RAM & ROM size, and modify RAM & ROM size at linker file. (e.g scatter file, lds file etc)

// <h> System

// <n> SCU (System Core Unit Core) included by default
// <q> SCU CLK (System Core Unit Clock)
//  <i> - A SCU Clock is one of the System Control Unit(SCU) subsystem.
//  <i> - This system provides high/low speed clock oscillated by internal oscillator and external oscillator.
//  <i> - There is a Phase-Locked Loop (PLL) block that oscillates based on the high speed internal/external clock for more faster system and peripheral operating. 
#define EX_MULTI_SCU_CLK     0

// <q> SCU LVD (System Core Unit Low Voltage Detector)
//  <i> - A SCU Low Voltage Detector (LVD) is one of the System Control Unit(SCU) subsystem.
//  <i> - This system provides to detect unstable power supply and notifies the danger or generates system reset itself.
#define EX_MULTI_SCU_LVD     0

// <q> SCU PWR (System Core Unit Power)
//  <i> - A SCU Power is one of the System Control Unit(SCU) subsystem.
//  <i> - This system consists of some operating mode such as INIT mode, RUN mode, SLEEP mode, DEEP-SLEEP mode, DEEP-SLEEP2 Mode.
//  <i>   * INIT mode : An Initial state of the chip when a reset is asserted.
//  <i>   * RUN mode : The maximum performance with high-speed clock system.
//  <i>   * SLEEP mode : Used as a low power consumption mode by halting processor core.
//  <i>   * DEEP-SLEEP and DEEP-SLEEP2 mode : Used as a low power consumption mode by halting processor core and unused peripherals.
#define EX_MULTI_SCU_PWR     0
// </h> System

// <h> Port 

// <q> PCU / GPIO (Port Control Unit / General Port Input Output)
//  <i> - Port Control Unit (PCU) supports the two major functions below.
//  <i> - Port Control
//  <i>  * Configures an external signal direction
//  <i>  * Configures Interrupt trigger mode
//  <i>  * Controls Internal pull-up/down and open-drain
//  <i> - GPIO
//  <i>  * Outputs signal level (H/L) on a pin
//  <i>  * Sets an external interrupt mode up (Level, Rising edge, Falling edge, Both edge)
//  <i>  * Disables/enables pull-up/down
#define EX_MULTI_PCU_GPIO    0

// </h> Port 

// <h> Timer Group

// <q> TIMER1 (16bit Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer1.
//  <i> - Timer1 receives the prescaled source clock and produces 16bit counter.
//  <i> - Timer1 has four operation modes below.
//  <i>  * Periodic mode
//  <i>  * Pulse Width Modulation(PWM) mode
//  <i>  * One-shot mode
//  <i>  * Capture mode
#define EX_MULTI_TIMER1      0

// <q> TIMER2 (32bit Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer2.
//  <i> - Timer2 receives the prescaled source clock and produces 32bit counter.
//  <i> - Timer2 has four operation modes below.
//  <i>  * Periodic mode
//  <i>  * Pulse Width Modulation(PWM) mode
//  <i>  * One-shot mode
//  <i>  * Capture mode
#define EX_MULTI_TIMER2      0

// <q> TIMER3 (16bit 3-phase Complementary PWM Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer3.
//  <i> - Timer3 receives the prescaled source clock and produces 16bit counter.
//  <i> - Timer3 has three operation modes below.
//  <i>  * Interval mode
//  <i>  * Back-to-Back mode
//  <i>  * Capture mode
//  <i> - Moreover, there are the three match data buffers which generate 3-phase complementary PWM signal.
#define EX_MULTI_TIMER3      0

// <q> TIMER4 (16bit XOR Capture Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer4.
//  <i> - Timer4 receives the prescaled source clock and produces 16bit counter.
//  <i> - Timer4 has four operation modes below.
//  <i>  * Periodic mode
//  <i>  * Pulse Width Modulation(PWM) mode
//  <i>  * One-shot mode
//  <i>  * Capture mode
//  <i> - Moreover, the Timer4 supports XORing input in capture mode.
#define EX_MULTI_TIMER4      0

// <q> TIMER4E (16bit 2-phase Complementary PWM Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer4E.
//  <i> - Timer4E receives the prescaled source clock and produces 16bit counter.
//  <i> - Timer4E has four operation modes below.
//  <i>  * Interval mode
//  <i>  * Back-to-Back mode
//  <i>  * Capture mode
//  <i>  * One-Shot Interval mode
//  <i> - Moreover, there are the two match data buffers which generate 2-phase complementary PWM signal.
#define EX_MULTI_TIMER4E     0

// <q> TIMER5 (16bit Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer5.
//  <i> - Timer5 receives the prescaled source clock and produces 16bit counter.
//  <i> - Timer5 has four operation modes below.
//  <i>  * Periodic mode
//  <i>  * Pulse Width Modulation(PWM) mode
//  <i>  * One-shot mode
//  <i>  * Capture mode
#define EX_MULTI_TIMER5      0

// <q> TIMER6 (100Hz Clock 16bit Timer)
//  <i> - The source clock prescaled by the dedicated prescaler is fed to Timer6.
//  <i> - Timer6 receives the prescaled 100Hz source clock and produces 16bit counter.
//  <i> - Timer6 has an operation mode below.
//  <i>  * Periodic mode
#define EX_MULTI_TIMER6      0

// <q> WDT (Watch-Dog Timer)
//  <i> - A Watch-Dog Timer (WDT) rapidly detects CPU's malfunction such as endless looping caused by noise, and generates WDT signal.
//  <i> - The WDT signal can be used for resetting CPU or triggering interrupt.
//  <i> - If WDT function is disabled, it can be used as a normal timer to generate an interrupt at a fixed interval. 
#define EX_MULTI_WDT         0

// <q> WT (Watch Timer)
//  <i> - Watch Timer (WT) is a 26bit timer and supports generating a event at a certain interval and an accurate delay.
#define EX_MULTI_WT          0

// <q> RTC (Real-Time Clock)
//  <i> - Real-Time Clock (RTC) is an internal block that measures the passage of time.
//  <i> - It must keep accurate time-of-day.
//  <i> - Programmable alarm or automatic wake-up can be implemented by it.
#define EX_MULTI_RTC         0

// <q> MPWM (Motor Pulse Width Modulation)
//  <i> - Motor Pulse Width Modulation (MPWM) is a sort of ABOV terminology and a special PWM supporting motor control.
//  <i> - It has three operation modes below.
//  <i>  * Motor PWM mode for Motor control
//  <i>  * Normal PWM mode for a variety of PWM applications
//  <i>  * Individual PWM mode for specific applications like IH cooker.
//  <i> - Moreover, dead-time for half-bridge circuit and, detecting external protection and over-voltage event are supported.
#define EX_MULTI_MPWM        0

// <q> FRT (Free-Run Timer)
//  <i> - Free-Run Timer (FRT) is a 32-bit up-count timer and generating a event at a certain interval.
#define EX_MULTI_FRT         0

// </h> Timer Group

// <h> Connectivity & Communication Group

// <q> I2C (Inter-Integrated Circuit)
//  <i> - Inter-Integrated Circuit (I2C) is a standard protocol using two wires to attach lower-speed peripheral ICs to processors/microcontroller.
//  <i> - It supprots transmitting and receiving data with Serial Data Line (SDA) and Serial Clock Line(SCL).
#define EX_MULTI_I2C         0

// <q> SPI (Serial Peripheral Interface)
//  <i> - Serial Peripheral Interface (SPI) is a synchronous serial communication interface specification used for short-distance communication using the four wires bleow.
//  <i>  * SS : chip select
//  <i>  * SCK : serial clock
//  <i>  * MOSI : master out slave in
//  <i>  * MISO : master in slave out
//  <i> - Master/slave mode and full-duplex communication are supported  
#define EX_MULTI_SPI         0

// <q> UART (Universal Asynchronus Receiver Transmitter)
//  <i> - Universal Asynchronous Receiver/Transmitter (UART) is asynchronous serial communication protocol.
//  <i> - It is capable of a full-duplex asynchronous serial communication by configuring baud-rate, data bit, parity bit and stop bit.
//  <i> - The communiation status is available via line status information such as Parity error, Overrun, Frame error.
#define EX_MULTI_UART        0

// <q> LPUART (Low Power Universal Asynchronus Receiver Transmitter)
//  <i> - Low Power Universal Asynchronous Receiver/Transmitter (LPUART) is asynchronous serial communication protocol.
//  <i> - It is capable of a half and full-duplex asynchronous serial communication by configuring baud-rate, data bit, parity bit and stop bit.
//  <i> - The communiation status is available via line status information such as Parity error, Overrun, Frame error.
#define EX_MULTI_LPUART      0

// <q> USART (Universal Synchronous and Asynchronous Receiver Transmitter)
//  <i> - Universal Synchronous and Asynchronous Receiver/Transmitter (USART) is synchronus or asynchronus communication protocol.
//  <i> - It is capable of three different serial communication modes below.
//  <i>  * UART
//  <i>  * USRT
//  <i>  * SPI
#define EX_MULTI_USART       0

// </h> Connectivity & Communication Group

// <h> Analog Group

// <q> ADC (Analog-to-Digital Converter)
//  <i> - Analog-to-Digital Converter (ADC) converts an analog signal into a digital signal.
//  <i> - It has N-bit resolution and multiple indedpendent channels on which the converting operates.
//  <i> - Moreover, it is triggered by the specific interrupt of Timer or MPWM sampling a voltage level on the channel.
#define EX_MULTI_ADC         0

// <q> DAC (Digital-to-Analog Converter)
//  <i> - Digital-to-Analog Converter (DAC) converts a digital signal into an analog signal.
//  <i> - N-bit digital value is transformed to an analog output based on successive approximation logic.
#define EX_MULTI_DAC         0

// <q> CMP (Comparator)
//  <i> - Comparator compares levels of both an input analogue voltage and an external reference voltage/internal reference voltage.
//  <i> - It provides an binary value as the comparision result.
#define EX_MULTI_CMP         0

// <q> TEMP SENSOR (Temperature Sensor)
//  <i> - Temperature Sensor detects temperature variation by comparing counter of both sensing and reference clock.
#define EX_MULTI_TEMPSENS    0

// </h> Analog Group

// <h> Security & Integrity Group
// <q> CRC (Cyclic Redundancy Check)
//  <i> - Cyclic Redundancy Check (CRC) generates an error-detection code which is calcucated based on polynomials.
//  <i> - The error-detection code is used to examine the integrity of data transmission, storage or flash memory.
//  <i> - CRC-7, CRC-8, CRC-16 and CRC32 can be available.
#define EX_MULTI_CRC         0

// <q> AES (Advanced Encryption Standard)
//  <i> - Advanced Encryption Standard (AES) encrypts and decrypts data based on a symmetric-key alogrithm.
//  <i> - It is used to protect private or sensitive information.
//  <i> - Size of data block and key are 128bit.
#define EX_MULTI_AES         0

// <q> TRNG (True Random Number Generator)
//  <i> - True Random Number Generator (RNG) generates a random number from on analog noise sources
//  <i> - Because a random number has to be unpredicatable, it is used for cryptography function or a variety of security applications.
#define EX_MULTI_TRNG        0

// </h> Security & Integrity Group

// <h> Memory Group

// <q> FLASH (Stand-Alone Example)
//  <i> - FMC
//  <i>  * Flash Memory Controller (FMC) interfaces an internal code and data flash.
//  <i>  * It is composed of Code Flash Memory Controller (CFCM) and Data Flash Memory Controller (DFMC) for the interfacees.
//  <i>  * The internal code/data flash is supposed to store execution code/data binaries basically.
//  <i>  * For a certain fuctionality, it can access System Flash located at inside of a device.
//  <i> - CFMC
//  <i>  * Code Flash Memory Controller (CFMC) is flash memory controller to interface an internal code flash.
//  <i>  * The internal code flash is supposed to store execution code binaries basically. 
//  <i>  * For a certain fuctionality, it can access System Flash located at inside of a device.
#define EX_MULTI_FLASH       0

// <q> COA (Configuration Option Area : Only Watch-dog Timer and Low Voltage Reset)
//  <i> - Configuration Option Area (COA) is used for configuring WDT and LVR system option.
#define EX_MULTI_COA         0

// <q> EBI (External Bus Interface)
//  <i> - External Bus Interface (EBI) is a sort of bus interfacing small memory mapped I/O devices such as flash memory, SRAM or etc.
#define EX_MULTI_EBI         0

// </h> Memory Group

// <h> Display Group

// <q> LCD (Liquied Crystal Display)
//  <i> - Liquied Crystal Display (LCD) controller is capable of driving Mono Twisted Nematic(TN)/Super Twisted Nematic(STN) LCD device.
#define EX_MULTI_LCD         0

// <q> LED (Light Emitting Diode)
//  <i> - Light Emitting Diode (LED) controller drives common anode and cathode of a display device.
//  <i> - Common(ICOM) and segment(ISEG) interface the display device mainly.
#define EX_MULTI_LED         0

// </h> Display Group

// <h> Miscellaneous Group

// <q> HSICMU (High-Speed Internal OSC Clock Management Unit)
//  <i> - The High-Speed Internal OSC Clock Management Unit (HSICMU) is designed for the auto-trimming of the Internal OSC clock.
#define EX_MULTI_HSICMU      0

// </h> Miscellaneous Group

// </h> End of ABOV example configuration

/*
//------------- <<< end of configuration section >>> --------------------------
*/


/* Define customized target example */
#if defined(_RTE_) || (TARGET_BOARD & BD_STK_MASK)
/* Define target example */
#define GPIO_LED_BLINKING

#else
#if (EX_MULTI_SCU_CLK == 1)
#define SCU_CLK_TC
#endif
#if (EX_MULTI_SCU_LVD == 1)
#define SCU_LVD_TC
#endif
#if (EX_MULTI_SCU_PWR == 1)
#define SCU_PWR_TC
#endif
#if (EX_MULTI_PCU_GPIO == 1)
#define PCU_TC
#endif
#if (EX_MULTI_TIMER1 == 1)
#define TIMER1_TC
#endif
#if (EX_MULTI_TIMER2 == 1)
#define TIMER2_TC
#endif
#if (EX_MULTI_TIMER3 == 1)
#define TIMER3_TC
#endif
#if (EX_MULTI_TIMER4 == 1)
#define TIMER4_TC
#endif
#if (EX_MULTI_TIMER4E == 1)
#define TIMER4E_TC
#endif
#if (EX_MULTI_TIMER5 == 1)
#define TIMER5_TC
#endif
#if (EX_MULTI_TIMER6 == 1)
#define TIMER6_TC
#endif
#if (EX_MULTI_WDT == 1)
#define WDT_TC
#endif
#if (EX_MULTI_WT == 1)
#define WT_TC
#endif
#if (EX_MULTI_MPWM == 1)
#define MPWM_TC
#endif
#if (EX_MULTI_I2C == 1)
#define I2C_TC
#endif
#if (EX_MULTI_SPI == 1)
#define SPI_TC
#endif
#if (EX_MULTI_UART == 1)
#define UART_TC
#endif
#if (EX_MULTI_LPUART == 1)
#define LPUART_TC
#endif
#if (EX_MULTI_USART == 1)
#define USART_TC
#endif
#if (EX_MULTI_ADC == 1)
#define ADC_TC
#endif
#if (EX_MULTI_DAC == 1)
#define DAC_TC
#endif
#if (EX_MULTI_CMP == 1)
#define CMP_TC
#endif
#if (EX_MULTI_CRC == 1)
#define CRC_TC
#endif
#if (EX_MULTI_AES == 1)
#define AES_TC
#endif
#if (EX_MULTI_TRNG == 1)
#define TRNG_TC
#endif
#if (EX_MULTI_FLASH == 1)
#define FLASH_TC
#endif
#if (EX_MULTI_LCD == 1)
#define LCD_TC
#endif
#if (EX_MULTI_LED == 1)
#define LED_TC
#endif
#if (EX_MULTI_EBI == 1)
#define EBI_TC
#endif
#if (EX_MULTI_RTC == 1)
#define RTC_TC
#endif
#if (EX_MULTI_TEMPSENS == 1)
#define TEMPSENS_TC
#endif
#if (EX_MULTI_COA == 1)
#define COA_TC
#endif
#if (EX_MULTI_FRT == 1)
#define FRT_TC
#endif
#if (EX_MULTI_HSICMU == 1)
#define HSICMU_TC
#endif

#endif

/* enable driver by XXX_TC */

/* default enable driver for debug */
#if (TARGET_BOARD_DEBUG_ENABLE == 1)
#if (TARGET_SERIAL_MODULE == BD_SERIAL_MODULE_NONE) || (TARGET_SERIAL_MODULE == BD_SERIAL_MODULE_USART) || defined (USART_TC)
#define CONFIG_HAL_USART       1
#endif
#if (TARGET_SERIAL_MODULE == BD_SERIAL_MODULE_UART) || defined (UART_TC)
#define CONFIG_HAL_UART        1
#endif
#endif
#define CONFIG_HAL_DMAC        1

#if defined(SCU_LVD_TC)
#define CONFIG_HAL_SCU_LVD     1
#endif

#if defined(SCU_PWR_TC)
#define CONFIG_HAL_SCU_PWR     1
#endif

#if defined(TIMER1_TC)
#define CONFIG_HAL_TIMER1      1
#endif

#if defined(TIMER2_TC)
#define CONFIG_HAL_TIMER2      1
#endif

#if defined(TIMER3_TC)
#define CONFIG_HAL_TIMER3      1
#endif

#if defined(TIMER4_TC)
#define CONFIG_HAL_TIMER4      1
#endif

#if defined(TIMER4E_TC)
#define CONFIG_HAL_TIMER4E     1
#endif

#if defined(TIMER5_TC)
#define CONFIG_HAL_TIMER5      1
#endif

#if defined(TIMER6_TC)
#define CONFIG_HAL_TIMER6      1
#endif

#if defined(WDT_TC)
#define CONFIG_HAL_WDT         1
#endif

#if defined(WT_TC)
#define CONFIG_HAL_WT          1
#endif

#if defined(MPWM_TC)
#define CONFIG_HAL_MPWM        1
#endif

#if defined(I2C_TC)
#define CONFIG_HAL_I2C         1
#endif

#if defined(SPI_TC)
#define CONFIG_HAL_SPI         1
#endif

#if defined(LPUART_TC)
#define CONFIG_HAL_LPUART      1
#endif

#if defined(ADC_TC)
#define CONFIG_HAL_ADC         1
#endif

#if defined(DAC_TC)
#define CONFIG_HAL_DAC         1
#endif

#if defined(CMP_TC)
#define CONFIG_HAL_CMP         1
#endif

#if defined(CRC_TC)
#define CONFIG_HAL_CRC         1
#endif

#if defined(AES_TC)
#define CONFIG_HAL_AES         1
#endif

#if defined(TRNG_TC)
#define CONFIG_HAL_TRNG        1
#endif

#if defined(FLASH_TC) || defined(COA_TC) || defined(CONFIG_MODULE_COA)
#define CONFIG_HAL_FMC         1
#endif

#if defined(LCD_TC)
#define CONFIG_HAL_LCD         1
#endif

#if defined(LED_TC)
#define CONFIG_HAL_LED         1
#endif

#if defined(EBI_TC)
#define CONFIG_HAL_EBI         1
#endif

#if defined(RTC_TC)
#define CONFIG_HAL_RTC         1
#endif

#if defined(TEMPSENS_TC)
#define CONFIG_HAL_TEMPSENS    1
#endif

#if defined(FRT_TC)
#define CONFIG_HAL_FRT         1
#endif

#if defined(HSICMU_TC)
#define CONFIG_HAL_HSICMU      1
#endif

#endif /* _EXAMPLE_CONFIG_H_ */
