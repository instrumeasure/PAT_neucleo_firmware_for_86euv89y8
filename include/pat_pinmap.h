/**
 * @file pat_pinmap.h
 * Locked MCU pin routing: legacy HAT **86euv89y8** J1, four ADS127L11 buses (SPI1–SPI4).
 *
 * Do not duplicate these GPIO / AF literals elsewhere — include this header and extend
 * `PINMAP.md` / `cube/legacy_86euv89y8_h753zi.ioc` only when intentionally changing hardware.
 */
#ifndef PAT_PINMAP_H
#define PAT_PINMAP_H

#include "stm32h7xx_hal.h"

/* ------------------------------------------------------------------ */
/* Shared ADS127 control (all channels)                               */
/* ------------------------------------------------------------------ */

#define PAT_PINMAP_ADS127_START_PORT  GPIOF
#define PAT_PINMAP_ADS127_START_PIN   GPIO_PIN_1
#define PAT_PINMAP_ADS127_NRESET_PORT GPIOF
#define PAT_PINMAP_ADS127_NRESET_PIN  GPIO_PIN_0

/* ------------------------------------------------------------------ */
/* SPI1 — channel 0                                                   */
/* ------------------------------------------------------------------ */
/*
 * NUCLEO-H753ZI caveat: “School-bus” **SPI1 on PA5/PA6/PA7** clashes with **Ethernet RMII**
 * (PA7 is a PHY line on the Nucleo) — see:
 * https://community.st.com/t5/stm32-mcus-embedded-software/having-a-hardtime-to-get-spi1-working-on-stm32h753zi/td-p/140024
 * PAT **86euv89y8** J1 uses **SPI1** on **PG11 (SCK), PD7 (MOSI), PG9 (MISO), PA4 (!CS)** instead.
 * (H7 LL note from same thread: 8-bit frames use an **8-bit store** to the data register, not a raw 32-bit TXDR write.)
 */

#define PAT_PINMAP_SPI1_NCS_PORT   GPIOA
#define PAT_PINMAP_SPI1_NCS_PIN    GPIO_PIN_4
#define PAT_PINMAP_SPI1_SCK_PORT   GPIOG
#define PAT_PINMAP_SPI1_SCK_PIN    GPIO_PIN_11
#define PAT_PINMAP_SPI1_MOSI_PORT  GPIOD
#define PAT_PINMAP_SPI1_MOSI_PIN   GPIO_PIN_7
#define PAT_PINMAP_SPI1_MISO_PORT  GPIOG
#define PAT_PINMAP_SPI1_MISO_PIN   GPIO_PIN_9
#define PAT_PINMAP_SPI1_SCK_AF     GPIO_AF5_SPI1
#define PAT_PINMAP_SPI1_MOSI_AF    GPIO_AF5_SPI1
#define PAT_PINMAP_SPI1_MISO_AF    GPIO_AF5_SPI1

/* ------------------------------------------------------------------ */
/* SPI2 — channel 1                                                   */
/* ------------------------------------------------------------------ */
/*
 * MISO ball: STM32CubeMX often labels this pad **PC2_C** (H753 dual-pad / analog-switch family).
 * CMSIS/HAL name is still **PC2** on **GPIOC** → `GPIO_PIN_2`. Same physical pin as Cube “PC2_C”.
 *
 * ST (NUCLEO-H753ZI / LQFP144): **PC2_C** is the package ball; the separate **PC2**-only pad exists
 * only on **TFBGA240+25** — see ST Community thread:
 * https://community.st.com/t5/stm32-mcus-products/subject-stm32h753zi-cannot-control-pc02-as-gpio-or-spi2-miso/td-p/655439
 * That thread’s outcome: ST could toggle **PC2_C** on Nucleo; “stuck low” cases were treated as board /
 * continuity / damaged IO, not a generic PC2 silicon bug. ST’s GPIO test used **SYSCFG_SWITCH_PC2_CLOSE**
 * (`HAL_SYSCFG_AnalogSwitchConfig`) before driving the pin — align with `ads127_pins_init()` **PC2SO**
 * default (switch closed for digital).
 *
 * Related (H735 **LQFP100**, SPI through **PC2_C** / **PC3_C** with switches closed): very slow edges /
 * high effective series R through the analog switch (~300 Ω class), VDDA-powered switch, fragile **I_O**
 * limits on **Pxy_C** — one report cites ST calling a **silicon issue** pending errata; others saw OK on
 * Nucleo-H723. Useful background only; PAT Nucleo **LQFP144** routes SPI2 MISO on this ball with **PB15** MOSI.
 * https://community.st.com/t5/stm32-mcus-products/stm32h735v-lqfp100-pc2-c-and-pc3-c-speed/td-p/214053
 *
 * **H753VIT6** / **PC2_C** as SPI2 MISO: ST recommends checking **SYSCFG_PMCR.PC2SO** (switch) and cites
 * **datasheet + errata ES0392** — keep **external load** on **Pxy_C** pins within rated **I_O** (thread
 * highlights **≤ 1 mA** class concern); measure if MISO looks stuck (e.g. 0xFF) while SCLK is active.
 * https://community.st.com/t5/stm32-mcus-products/stm32h753vit6-pc2-c-pin/td-p/739240
 */

#define PAT_PINMAP_SPI2_NCS_PORT   GPIOB
#define PAT_PINMAP_SPI2_NCS_PIN    GPIO_PIN_4
#define PAT_PINMAP_SPI2_SCK_PORT   GPIOB
#define PAT_PINMAP_SPI2_SCK_PIN    GPIO_PIN_10
#define PAT_PINMAP_SPI2_MOSI_PORT  GPIOB
#define PAT_PINMAP_SPI2_MOSI_PIN   GPIO_PIN_15
#define PAT_PINMAP_SPI2_MISO_PORT  GPIOC
#define PAT_PINMAP_SPI2_MISO_PIN   GPIO_PIN_2
#define PAT_PINMAP_SPI2_AF         GPIO_AF5_SPI2

/* ------------------------------------------------------------------ */
/* SPI3 — channel 2                                                   */
/* ------------------------------------------------------------------ */

#define PAT_PINMAP_SPI3_NCS_PORT   GPIOA
#define PAT_PINMAP_SPI3_NCS_PIN    GPIO_PIN_15
#define PAT_PINMAP_SPI3_SCK_PORT   GPIOC
#define PAT_PINMAP_SPI3_SCK_PIN    GPIO_PIN_10
#define PAT_PINMAP_SPI3_MOSI_PORT  GPIOD
#define PAT_PINMAP_SPI3_MOSI_PIN   GPIO_PIN_6
#define PAT_PINMAP_SPI3_MISO_PORT  GPIOC
#define PAT_PINMAP_SPI3_MISO_PIN   GPIO_PIN_11
#define PAT_PINMAP_SPI3_SCK_MISO_AF GPIO_AF6_SPI3
#define PAT_PINMAP_SPI3_MOSI_AF     GPIO_AF5_SPI3

/* ------------------------------------------------------------------ */
/* SPI4 — channel 3                                                   */
/* ------------------------------------------------------------------ */

#define PAT_PINMAP_SPI4_NCS_PORT   GPIOE
#define PAT_PINMAP_SPI4_NCS_PIN    GPIO_PIN_11
#define PAT_PINMAP_SPI4_SCK_PORT   GPIOE
#define PAT_PINMAP_SPI4_SCK_PIN    GPIO_PIN_12
#define PAT_PINMAP_SPI4_MOSI_PORT  GPIOE
#define PAT_PINMAP_SPI4_MOSI_PIN   GPIO_PIN_6
#define PAT_PINMAP_SPI4_MISO_PORT  GPIOE
#define PAT_PINMAP_SPI4_MISO_PIN   GPIO_PIN_13
/** Same SDO/!DRDY net as PE13 MISO; GPIO-only so DRDY arm polls IDR without flipping PE13 MODER off AF5. */
#define PAT_PINMAP_SPI4_MISO_DRDY_SENSE_PORT  GPIOE
#define PAT_PINMAP_SPI4_MISO_DRDY_SENSE_PIN    GPIO_PIN_15
#define PAT_PINMAP_SPI4_AF         GPIO_AF5_SPI4
#define PAT_PINMAP_SPI4_AF_PINS \
  (PAT_PINMAP_SPI4_SCK_PIN | PAT_PINMAP_SPI4_MOSI_PIN | PAT_PINMAP_SPI4_MISO_PIN)

/* ------------------------------------------------------------------ */
/* SPI6 — J2 inter-HAT slave (host is master), AF8                     */
/* ------------------------------------------------------------------ */

#define PAT_PINMAP_SPI6_SCK_PORT   GPIOA
#define PAT_PINMAP_SPI6_SCK_PIN    GPIO_PIN_5
#define PAT_PINMAP_SPI6_NSS_PORT   GPIOG
#define PAT_PINMAP_SPI6_NSS_PIN    GPIO_PIN_8
#define PAT_PINMAP_SPI6_MISO_PORT  GPIOG
#define PAT_PINMAP_SPI6_MISO_PIN   GPIO_PIN_12
#define PAT_PINMAP_SPI6_MOSI_PORT  GPIOG
#define PAT_PINMAP_SPI6_MOSI_PIN   GPIO_PIN_14
#define PAT_PINMAP_SPI6_AF         GPIO_AF8_SPI6
#define PAT_PINMAP_SPI6_GPIOG_AF_PINS \
  (PAT_PINMAP_SPI6_NSS_PIN | PAT_PINMAP_SPI6_MISO_PIN | PAT_PINMAP_SPI6_MOSI_PIN)

#endif /* PAT_PINMAP_H */
