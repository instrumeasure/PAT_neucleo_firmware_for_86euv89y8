#ifndef PAT_SPI_H7_MASTER_H
#define PAT_SPI_H7_MASTER_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

/**
 * FIFO sizing for H7 SPI v2 master polled TX/RX (`pat_spi_h7_master_txrx`).
 * Fill with `pat_spi_h7_master_cfg_from_hspi()` after `HAL_SPI_Init`.
 *
 * **Ownership:** after init, do not call `HAL_SPI_TransmitReceive` / `_IT` / `_DMA`
 * on the same `SPIx` — this module drives `TSIZE` / `CSTART` / `EOT` directly.
 */
typedef struct {
  uint32_t fifo_len;
  uint16_t fifo_pkt;
} pat_spi_master_cfg_t;

void pat_spi_h7_master_cfg_from_hspi(const SPI_HandleTypeDef *hs, pat_spi_master_cfg_t *cfg);

/**
 * Polled master transfer: writes `len` bytes from `tx` (use NULL to clock out zeros),
 * reads `len` bytes into `rx`. Uses `SPI_CR2_TSIZE` + `CSTART` + RXP/TXP/EOT like
 * `stm32h7xx_hal_spi.c`, with **DWT cycle** timeout (not `HAL_GetTick`).
 *
 * @param timeout_cycles `UINT32_MAX` = wait indefinitely; else abort after this many
 *        DWT cycles from transfer start (ensure `CoreDebug->DEMCR` / `DWT->CTRL` enabled).
 */
HAL_StatusTypeDef pat_spi_h7_master_txrx(SPI_TypeDef *SPIx, const pat_spi_master_cfg_t *cfg,
    const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_cycles);

/**
 * Four parallel H7 SPI v2 masters: 3-byte zero-TX reads on SPI1..SPI4 with **interleaved**
 * register polling so **SCLK on all buses can overlap** (no `HAL_SPI_TransmitReceive_IT`).
 * Preconditions: each `hN` initialised by `HAL_SPI_Init`; **SPE** may be on; caller issues
 * no HAL SPI transfer on these instances during the call.
 */
HAL_StatusTypeDef pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi(SPI_HandleTypeDef *h0, SPI_HandleTypeDef *h1,
    SPI_HandleTypeDef *h2, SPI_HandleTypeDef *h3, uint8_t *rx0, uint8_t *rx1, uint8_t *rx2, uint8_t *rx3,
    uint32_t timeout_cycles);

#endif
