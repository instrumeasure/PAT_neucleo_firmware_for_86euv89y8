/**
 * @file pat_clock.h
 * @brief NUCLEO-H753ZI clock tree: HSI + PLL1 (same strategy as legacy SPI test harness).
 *
 * Call after HAL_Init(), then peripherals may use stable kernel clocks and HAL_Delay is accurate
 * (see stm32h7-hal-pitfalls: re-init tick after PLL).
 */
#ifndef PAT_CLOCK_H
#define PAT_CLOCK_H

void PAT_SystemClock_Config(void);

#endif
