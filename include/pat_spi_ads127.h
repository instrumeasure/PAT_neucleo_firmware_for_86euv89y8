#ifndef PAT_SPI_ADS127_H
#define PAT_SPI_ADS127_H

#include "stm32h7xx_hal.h"

/**
 * SPI1–3: default SPI123 kernel **÷64** (override on `pat_nucleo_quartet` via CMake `PAT_SPI123_PRESCALER_DIV`).
 * SPI4: SPI4 kernel **÷16**. Other ELFs use defaults here.
 */
uint32_t pat_spi_ads127_prescaler_for_instance(const SPI_TypeDef *instance);

/**
 * Zero-fill handle, apply ADS127-oriented master template, `HAL_SPI_Init`.
 * Ensures `HAL_SPI_MspInit` runs per `Instance` (HAL state starts at RESET).
 */
HAL_StatusTypeDef pat_spi_ads127_apply_template(SPI_HandleTypeDef *hspi, SPI_TypeDef *instance);

#endif
