#ifndef PAT_UART7_LASER_H
#define PAT_UART7_LASER_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define PAT_LASER_STATUS_BLOB_MAX 496u

typedef struct pat_laser_status_cache {
  uint32_t last_poll_ms;
  uint16_t status_len;
  uint16_t cache_flags;
  uint8_t status_blob[PAT_LASER_STATUS_BLOB_MAX];
} pat_laser_status_cache_t;

typedef struct pat_uart7_laser_ctx {
  UART_HandleTypeDef *huart7;
  DMA_HandleTypeDef *hdma_rx;
  DMA_HandleTypeDef *hdma_tx;
  volatile uint8_t busy;
  volatile uint8_t rx_event_ready;
  volatile uint16_t rx_event_len;
  uint8_t rx_dma_buf[PAT_LASER_STATUS_BLOB_MAX];
  pat_laser_status_cache_t cache;
} pat_uart7_laser_ctx_t;

void pat_uart7_laser_init(pat_uart7_laser_ctx_t *ctx, UART_HandleTypeDef *huart7, DMA_HandleTypeDef *hdma_rx, DMA_HandleTypeDef *hdma_tx);
HAL_StatusTypeDef pat_uart7_start_rx_dma(pat_uart7_laser_ctx_t *ctx);
void pat_uart7_on_rx_event(pat_uart7_laser_ctx_t *ctx, uint16_t size);
void pat_uart7_poll_parser(pat_uart7_laser_ctx_t *ctx);
HAL_StatusTypeDef pat_uart7_bypass_exchange(pat_uart7_laser_ctx_t *ctx, const uint8_t *tx, uint16_t tx_n, uint8_t *rx, uint16_t *rx_n, uint32_t timeout_ms);
HAL_StatusTypeDef pat_uart7_status_tick(pat_uart7_laser_ctx_t *ctx, uint32_t now_ms, uint32_t period_ms);

#endif
