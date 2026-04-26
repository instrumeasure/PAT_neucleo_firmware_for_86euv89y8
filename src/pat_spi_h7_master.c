#include "pat_spi_h7_master.h"
#include <stdint.h>

void pat_spi_h7_master_cfg_from_hspi(const SPI_HandleTypeDef *hs, pat_spi_master_cfg_t *cfg)
{
  SPI_TypeDef *const SPIx = hs->Instance;
  cfg->fifo_len = IS_SPI_HIGHEND_INSTANCE(SPIx) ? SPI_HIGHEND_FIFO_SIZE : SPI_LOWEND_FIFO_SIZE;
  cfg->fifo_pkt = (uint16_t)(((uint16_t)(hs->Init.FifoThreshold >> 5U) + 1U));
}

/* HAL SPI register macros used here only need Instance; avoid memset per bus. */
static void pat_spi_h7_stub_hspi(SPI_HandleTypeDef *hs, SPI_TypeDef *SPIx)
{
  hs->Instance = SPIx;
}

static void pat_spi_h7_force_abort_cleanup(SPI_TypeDef *SPIx)
{
  SPI_HandleTypeDef hs;
  pat_spi_h7_stub_hspi(&hs, SPIx);
  MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, 0UL);
  __HAL_SPI_CLEAR_EOTFLAG(&hs);
  __HAL_SPI_CLEAR_TXTFFLAG(&hs);
  __HAL_SPI_DISABLE(&hs);
  __HAL_SPI_DISABLE_IT(&hs, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR
                             | SPI_IT_FRE | SPI_IT_MODF));
  CLEAR_BIT(SPIx->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);
}

static void pat_spi_dwt_ensure(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0u) {
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }
}

HAL_StatusTypeDef pat_spi_h7_master_txrx(SPI_TypeDef *SPIx, const pat_spi_master_cfg_t *cfg,
    const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_cycles)
{
  if ((SPIx == NULL) || (cfg == NULL) || (rx == NULL) || (len == 0u)) {
    return HAL_ERROR;
  }

  pat_spi_dwt_ensure();

  SPI_HandleTypeDef hs;
  pat_spi_h7_stub_hspi(&hs, SPIx);

  /* Each transfer ends with `__HAL_SPI_DISABLE` (match prior HAL cleanup); re-arm SPE for the next frame. */
  __HAL_SPI_ENABLE(&hs);
  __DSB();

  const uint32_t fifo_len = cfg->fifo_len;
  const uint16_t fifo_pkt = cfg->fifo_pkt;

  uint16_t tx_left = len;
  uint16_t rx_left = len;
  uint16_t tx_idx = 0u;
  uint16_t rx_idx = 0u;

  MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, (uint32_t)len);
  SET_BIT(SPIx->CR1, SPI_CR1_CSTART);

  const uint32_t tlim = timeout_cycles;
  const uint32_t c0 = DWT->CYCCNT;

  while ((tx_left > 0u) || (rx_left > 0u)) {
    if (__HAL_SPI_GET_FLAG(&hs, SPI_FLAG_TXP) && (tx_left > 0u) && (rx_left < (tx_left + fifo_len))) {
      const uint8_t v = (tx != NULL) ? tx[tx_idx] : 0u;
      *((__IO uint8_t *)&SPIx->TXDR) = v;
      tx_idx++;
      tx_left--;
    }
    const uint32_t sr = SPIx->SR;
    if (rx_left > 0u) {
      if (__HAL_SPI_GET_FLAG(&hs, SPI_FLAG_RXP)) {
        rx[rx_idx++] = *((__IO uint8_t *)&SPIx->RXDR);
        rx_left--;
      } else if ((rx_left < fifo_pkt) && ((sr & SPI_SR_RXWNE_Msk) != 0UL)) {
        rx[rx_idx++] = *((__IO uint8_t *)&SPIx->RXDR);
        rx_left--;
      } else if ((rx_left < 4u) && ((sr & SPI_SR_RXPLVL_Msk) != 0UL)) {
        rx[rx_idx++] = *((__IO uint8_t *)&SPIx->RXDR);
        rx_left--;
      } else {
        if (tlim != UINT32_MAX) {
          if ((uint32_t)(DWT->CYCCNT - c0) > tlim) {
            pat_spi_h7_force_abort_cleanup(SPIx);
            return HAL_TIMEOUT;
          }
        }
      }
    }
  }

  while (!__HAL_SPI_GET_FLAG(&hs, SPI_FLAG_EOT)) {
    if (tlim != UINT32_MAX) {
      if ((uint32_t)(DWT->CYCCNT - c0) > tlim) {
        pat_spi_h7_force_abort_cleanup(SPIx);
        return HAL_TIMEOUT;
      }
    }
  }

  __HAL_SPI_CLEAR_EOTFLAG(&hs);
  __HAL_SPI_CLEAR_TXTFFLAG(&hs);
  __HAL_SPI_DISABLE(&hs);
  __HAL_SPI_DISABLE_IT(&hs, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR
                             | SPI_IT_FRE | SPI_IT_MODF));
  CLEAR_BIT(SPIx->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);
  MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, 0UL);
  return HAL_OK;
}

#define PAT_SPI_Q_N 4u

HAL_StatusTypeDef pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi(SPI_HandleTypeDef *h0, SPI_HandleTypeDef *h1,
    SPI_HandleTypeDef *h2, SPI_HandleTypeDef *h3, uint8_t *rx0, uint8_t *rx1, uint8_t *rx2, uint8_t *rx3,
    uint32_t timeout_cycles)
{
  SPI_HandleTypeDef *const hs_line[PAT_SPI_Q_N] = {h0, h1, h2, h3};
  uint8_t *const rx_line[PAT_SPI_Q_N] = {rx0, rx1, rx2, rx3};
  pat_spi_master_cfg_t cfgs[PAT_SPI_Q_N];
  SPI_HandleTypeDef stubs[PAT_SPI_Q_N];
  SPI_TypeDef *spi[PAT_SPI_Q_N];
  uint16_t tx_left[PAT_SPI_Q_N];
  uint16_t rx_left[PAT_SPI_Q_N];
  uint16_t rx_idx[PAT_SPI_Q_N];
  /** 0 = run TX/RX one step; 1 = wait EOT + cleanup; 2 = finished */
  uint8_t phase[PAT_SPI_Q_N];

  pat_spi_dwt_ensure();

  for (unsigned i = 0u; i < PAT_SPI_Q_N; i++) {
    if ((hs_line[i] == NULL) || (rx_line[i] == NULL)) {
      return HAL_ERROR;
    }
    pat_spi_h7_master_cfg_from_hspi(hs_line[i], &cfgs[i]);
    spi[i] = hs_line[i]->Instance;
  }

  for (unsigned i = 0u; i < PAT_SPI_Q_N; i++) {
    pat_spi_h7_stub_hspi(&stubs[i], spi[i]);
    __HAL_SPI_ENABLE(&stubs[i]);
    __DSB();
    tx_left[i] = 3u;
    rx_left[i] = 3u;
    rx_idx[i] = 0u;
    phase[i] = 0u;
    MODIFY_REG(spi[i]->CR2, SPI_CR2_TSIZE, 3uL);
    SET_BIT(spi[i]->CR1, SPI_CR1_CSTART);
  }

  const uint32_t tlim = timeout_cycles;
  const uint32_t c0 = DWT->CYCCNT;
  unsigned done = 0u;

  while (done < PAT_SPI_Q_N) {
    unsigned progressed = 0u;

    for (unsigned i = 0u; i < PAT_SPI_Q_N; i++) {
      if (phase[i] == 2u) {
        continue;
      }

      SPI_TypeDef *const SPIx = spi[i];
      SPI_HandleTypeDef *const hs = &stubs[i];
      const uint32_t fifo_len = cfgs[i].fifo_len;
      const uint16_t fifo_pkt = cfgs[i].fifo_pkt;
      uint8_t *const prx = rx_line[i];

      if (phase[i] == 0u) {
        if ((tx_left[i] == 0u) && (rx_left[i] == 0u)) {
          phase[i] = 1u;
          continue;
        }

        if (__HAL_SPI_GET_FLAG(hs, SPI_FLAG_TXP) && (tx_left[i] > 0u) && (rx_left[i] < (tx_left[i] + fifo_len))) {
          *((__IO uint8_t *)&SPIx->TXDR) = 0u;
          tx_left[i]--;
          progressed = 1u;
          continue;
        }

        const uint32_t sr = SPIx->SR;
        if (rx_left[i] > 0u) {
          if (__HAL_SPI_GET_FLAG(hs, SPI_FLAG_RXP)) {
            prx[rx_idx[i]++] = *((__IO uint8_t *)&SPIx->RXDR);
            rx_left[i]--;
            progressed = 1u;
            continue;
          }
          if ((rx_left[i] < fifo_pkt) && ((sr & SPI_SR_RXWNE_Msk) != 0UL)) {
            prx[rx_idx[i]++] = *((__IO uint8_t *)&SPIx->RXDR);
            rx_left[i]--;
            progressed = 1u;
            continue;
          }
          if ((rx_left[i] < 4u) && ((sr & SPI_SR_RXPLVL_Msk) != 0UL)) {
            prx[rx_idx[i]++] = *((__IO uint8_t *)&SPIx->RXDR);
            rx_left[i]--;
            progressed = 1u;
            continue;
          }
        }
        continue;
      }

      if (phase[i] == 1u) {
        if (__HAL_SPI_GET_FLAG(hs, SPI_FLAG_EOT)) {
          __HAL_SPI_CLEAR_EOTFLAG(hs);
          __HAL_SPI_CLEAR_TXTFFLAG(hs);
          __HAL_SPI_DISABLE(hs);
          __HAL_SPI_DISABLE_IT(hs, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR
                                    | SPI_IT_FRE | SPI_IT_MODF));
          CLEAR_BIT(SPIx->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);
          MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, 0UL);
          phase[i] = 2u;
          done++;
          progressed = 1u;
        }
      }
    }

    if (tlim != UINT32_MAX) {
      if ((uint32_t)(DWT->CYCCNT - c0) > tlim) {
        for (unsigned j = 0u; j < PAT_SPI_Q_N; j++) {
          if (phase[j] != 2u) {
            pat_spi_h7_force_abort_cleanup(spi[j]);
          }
        }
        return HAL_TIMEOUT;
      }
    }

    if (progressed == 0u) {
      __NOP();
    }
  }

  return HAL_OK;
}
