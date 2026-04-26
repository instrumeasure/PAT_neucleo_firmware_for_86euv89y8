#include "pat_spi_ads127.h"
#include <string.h>

/* Quartet (and other targets) may override SPI1–3 prescaler via CMake; default ÷64. */
#if defined(PAT_SPI123_PRESCALER_8) && PAT_SPI123_PRESCALER_8
#define PAT_SPI123_BR SPI_BAUDRATEPRESCALER_8
#elif defined(PAT_SPI123_PRESCALER_16) && PAT_SPI123_PRESCALER_16
#define PAT_SPI123_BR SPI_BAUDRATEPRESCALER_16
#elif defined(PAT_SPI123_PRESCALER_32) && PAT_SPI123_PRESCALER_32
#define PAT_SPI123_BR SPI_BAUDRATEPRESCALER_32
#else
#define PAT_SPI123_BR SPI_BAUDRATEPRESCALER_64
#endif

uint32_t pat_spi_ads127_prescaler_for_instance(const SPI_TypeDef *instance)
{
  if (instance == SPI4) {
    return SPI_BAUDRATEPRESCALER_16;
  }
  return PAT_SPI123_BR;
}

HAL_StatusTypeDef pat_spi_ads127_apply_template(SPI_HandleTypeDef *hspi, SPI_TypeDef *instance)
{
  memset(hspi, 0, sizeof(*hspi));
  hspi->Instance = instance;
  hspi->Init.Mode = SPI_MODE_MASTER;
  hspi->Init.Direction = SPI_DIRECTION_2LINES;
  hspi->Init.DataSize = SPI_DATASIZE_8BIT;
  hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi->Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi->Init.NSS = SPI_NSS_SOFT;
  hspi->Init.BaudRatePrescaler = pat_spi_ads127_prescaler_for_instance(instance);
  hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi->Init.TIMode = SPI_TIMODE_DISABLE;
  hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi->Init.CRCPolynomial = 0x7U;
  hspi->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi->Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi->Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi->Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi->Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi->Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi->Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi->Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi->Init.IOSwap = SPI_IO_SWAP_DISABLE;
  return HAL_SPI_Init(hspi);
}
