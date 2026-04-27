#include "pat_uart7_laser.h"

#include <string.h>

/* Placeholder vendor status command; replace with SF8xxx frozen wire bytes. */
static const uint8_t k_status_query[] = {'S', 'T', 'A', 'T', 'U', 'S', '?', '\r', '\n'};

void pat_uart7_laser_init(pat_uart7_laser_ctx_t *ctx, UART_HandleTypeDef *huart7, DMA_HandleTypeDef *hdma_rx, DMA_HandleTypeDef *hdma_tx)
{
  if (ctx == NULL) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->huart7 = huart7;
  ctx->hdma_rx = hdma_rx;
  ctx->hdma_tx = hdma_tx;
}

HAL_StatusTypeDef pat_uart7_start_rx_dma(pat_uart7_laser_ctx_t *ctx)
{
  if ((ctx == NULL) || (ctx->huart7 == NULL)) {
    return HAL_ERROR;
  }
  return HAL_UARTEx_ReceiveToIdle_DMA(ctx->huart7, ctx->rx_dma_buf, sizeof(ctx->rx_dma_buf));
}

void pat_uart7_on_rx_event(pat_uart7_laser_ctx_t *ctx, uint16_t size)
{
  if (ctx == NULL) {
    return;
  }
  if (size > PAT_LASER_STATUS_BLOB_MAX) {
    size = PAT_LASER_STATUS_BLOB_MAX;
  }
  ctx->rx_event_len = size;
  ctx->rx_event_ready = 1u;
}

void pat_uart7_poll_parser(pat_uart7_laser_ctx_t *ctx)
{
  uint16_t n;
  if ((ctx == NULL) || (ctx->rx_event_ready == 0u)) {
    return;
  }
  ctx->rx_event_ready = 0u;
  n = ctx->rx_event_len;
  if (n > PAT_LASER_STATUS_BLOB_MAX) {
    n = PAT_LASER_STATUS_BLOB_MAX;
  }
  ctx->cache.status_len = n;
  if (n > 0u) {
    memcpy(ctx->cache.status_blob, ctx->rx_dma_buf, n);
    ctx->cache.cache_flags |= 0x1u;
    ctx->cache.cache_flags &= (uint16_t)~0x2u;
  }
}

HAL_StatusTypeDef pat_uart7_bypass_exchange(pat_uart7_laser_ctx_t *ctx, const uint8_t *tx, uint16_t tx_n, uint8_t *rx, uint16_t *rx_n, uint32_t timeout_ms)
{
  HAL_StatusTypeDef st;
  uint32_t t0;
  if ((ctx == NULL) || (ctx->huart7 == NULL) || (tx == NULL) || (rx == NULL) || (rx_n == NULL)) {
    return HAL_ERROR;
  }
  if (ctx->busy != 0u) {
    return HAL_BUSY;
  }
  ctx->busy = 1u;
  st = HAL_UART_Transmit(ctx->huart7, (uint8_t *)tx, tx_n, timeout_ms);
  if (st != HAL_OK) {
    ctx->busy = 0u;
    return st;
  }
  st = pat_uart7_start_rx_dma(ctx);
  if (st != HAL_OK) {
    ctx->busy = 0u;
    return st;
  }
  t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < timeout_ms) {
    pat_uart7_poll_parser(ctx);
    if (ctx->cache.status_len > 0u) {
      uint16_t n = ctx->cache.status_len;
      if (n > *rx_n) {
        n = *rx_n;
      }
      memcpy(rx, ctx->cache.status_blob, n);
      *rx_n = n;
      ctx->busy = 0u;
      return HAL_OK;
    }
  }
  ctx->cache.cache_flags |= 0x2u;
  ctx->busy = 0u;
  return HAL_TIMEOUT;
}

HAL_StatusTypeDef pat_uart7_status_tick(pat_uart7_laser_ctx_t *ctx, uint32_t now_ms, uint32_t period_ms)
{
  static uint32_t s_last = 0u;
  HAL_StatusTypeDef st;
  uint16_t rx_n = PAT_LASER_STATUS_BLOB_MAX;
  uint8_t rx[PAT_LASER_STATUS_BLOB_MAX];

  if (ctx == NULL) {
    return HAL_ERROR;
  }
  if ((now_ms - s_last) < period_ms) {
    return HAL_OK;
  }
  s_last = now_ms;
  st = pat_uart7_bypass_exchange(ctx, k_status_query, (uint16_t)sizeof(k_status_query), rx, &rx_n, 50u);
  if (st == HAL_OK) {
    ctx->cache.last_poll_ms = now_ms;
    ctx->cache.status_len = rx_n;
    memcpy(ctx->cache.status_blob, rx, rx_n);
    ctx->cache.cache_flags |= 0x1u;
    ctx->cache.cache_flags &= (uint16_t)~0x2u;
  } else {
    ctx->cache.last_poll_ms = now_ms;
    ctx->cache.cache_flags |= 0x2u;
  }
  return st;
}
