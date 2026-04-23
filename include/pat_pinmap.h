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
