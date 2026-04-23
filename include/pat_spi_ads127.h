#ifndef PAT_SPI_ADS127_H
#define PAT_SPI_ADS127_H

#include "stm32h7xx_hal.h"

/**
 * SPI1–3: SPI123 kernel /64 ~6.25 MHz SCLK. SPI4: SPI4 kernel /16 ~6.25 MHz.
 * Matches quartet / single-bus / SPI1–4 scan apps.
 */
uint32_t pat_spi_ads127_prescaler_for_instance(const SPI_TypeDef *instance);

/**
 * Zero-fill handle, apply ADS127-oriented master template, `HAL_SPI_Init`.
 * Ensures `HAL_SPI_MspInit` runs per `Instance` (HAL state starts at RESET).
 */
HAL_StatusTypeDef pat_spi_ads127_apply_template(SPI_HandleTypeDef *hspi, SPI_TypeDef *instance);

#endif
