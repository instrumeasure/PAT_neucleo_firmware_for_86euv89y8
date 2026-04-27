#include "pat_uart5_pat5.h"

#include "pat_crc32.h"
#include <string.h>

static uint16_t rb_next(uint16_t i)
{
  return (uint16_t)((i + 1u) % 1024u);
}

static uint16_t rb_count(const pat_uart5_rx_t *rx)
{
  if (rx->head >= rx->tail) {
    return (uint16_t)(rx->head - rx->tail);
  }
  return (uint16_t)(1024u - (rx->tail - rx->head));
}

static int rb_peek(const pat_uart5_rx_t *rx, uint16_t ofs, uint8_t *out)
{
  uint16_t c = rb_count(rx);
  uint16_t p;
  if (ofs >= c) {
    return -1;
  }
  p = (uint16_t)((rx->tail + ofs) % 1024u);
  *out = rx->buf[p];
  return 0;
}

static void rb_drop(pat_uart5_rx_t *rx, uint16_t n)
{
  while (n-- > 0u) {
    rx->tail = rb_next(rx->tail);
  }
}

void pat_uart5_rx_init(pat_uart5_rx_t *rx)
{
  if (rx == NULL) {
    return;
  }
  memset(rx, 0, sizeof(*rx));
}

void pat_uart5_rx_push(pat_uart5_rx_t *rx, const uint8_t *src, uint16_t n)
{
  uint16_t i;
  if ((rx == NULL) || (src == NULL)) {
    return;
  }
  for (i = 0u; i < n; i++) {
    rx->buf[rx->head] = src[i];
    rx->head = rb_next(rx->head);
    if (rx->head == rx->tail) {
      rx->tail = rb_next(rx->tail);
    }
  }
}

int pat_uart5_try_parse(pat_uart5_rx_t *rx, pat5_frame_t *out)
{
  uint8_t h[12];
  uint16_t i;
  uint16_t len;
  uint16_t full;
  uint8_t tmp[12 + PAT5_MAX_PAYLOAD + 4];
  uint32_t crc_rx;
  uint32_t crc_calc;

  if ((rx == NULL) || (out == NULL)) {
    return 0;
  }
  while (rb_count(rx) >= 4u) {
    if (rb_peek(rx, 0u, &h[0]) != 0 || rb_peek(rx, 1u, &h[1]) != 0 ||
        rb_peek(rx, 2u, &h[2]) != 0 || rb_peek(rx, 3u, &h[3]) != 0) {
      return 0;
    }
    if (h[0] != PAT5_MAGIC0 || h[1] != PAT5_MAGIC1 || h[2] != PAT5_MAGIC2 || h[3] != PAT5_MAGIC3) {
      rb_drop(rx, 1u);
      continue;
    }
    if (rb_count(rx) < 12u) {
      return 0;
    }
    for (i = 0u; i < 12u; i++) {
      (void)rb_peek(rx, i, &h[i]);
    }
    len = (uint16_t)((uint16_t)h[10] | ((uint16_t)h[11] << 8));
    if (len > PAT5_MAX_PAYLOAD) {
      rx->len_fail++;
      rb_drop(rx, 1u);
      continue;
    }
    full = (uint16_t)(12u + len + 4u);
    if (rb_count(rx) < full) {
      return 0;
    }
    for (i = 0u; i < full; i++) {
      (void)rb_peek(rx, i, &tmp[i]);
    }
    crc_rx = ((uint32_t)tmp[12u + len] | ((uint32_t)tmp[12u + len + 1u] << 8) |
              ((uint32_t)tmp[12u + len + 2u] << 16) | ((uint32_t)tmp[12u + len + 3u] << 24));
    crc_calc = pat_crc32_ieee(tmp, (size_t)(12u + len));
    if (crc_rx != crc_calc) {
      rx->crc_fail++;
      rb_drop(rx, 1u);
      continue;
    }
    out->ver = tmp[4];
    out->flags = tmp[5];
    out->seq = (uint16_t)((uint16_t)tmp[6] | ((uint16_t)tmp[7] << 8));
    out->cmd = (uint16_t)((uint16_t)tmp[8] | ((uint16_t)tmp[9] << 8));
    out->len = len;
    if (len > 0u) {
      memcpy(out->payload, &tmp[12], len);
    }
    rb_drop(rx, full);
    return 1;
  }
  return 0;
}

HAL_StatusTypeDef pat_uart5_send(UART_HandleTypeDef *huart, const pat5_frame_t *fr)
{
  uint8_t tx[12 + PAT5_MAX_PAYLOAD + 4];
  uint32_t crc;
  uint16_t n;
  if ((huart == NULL) || (fr == NULL) || (fr->len > PAT5_MAX_PAYLOAD)) {
    return HAL_ERROR;
  }
  tx[0] = PAT5_MAGIC0;
  tx[1] = PAT5_MAGIC1;
  tx[2] = PAT5_MAGIC2;
  tx[3] = PAT5_MAGIC3;
  tx[4] = fr->ver;
  tx[5] = fr->flags;
  tx[6] = (uint8_t)(fr->seq & 0xFFu);
  tx[7] = (uint8_t)((fr->seq >> 8) & 0xFFu);
  tx[8] = (uint8_t)(fr->cmd & 0xFFu);
  tx[9] = (uint8_t)((fr->cmd >> 8) & 0xFFu);
  tx[10] = (uint8_t)(fr->len & 0xFFu);
  tx[11] = (uint8_t)((fr->len >> 8) & 0xFFu);
  if (fr->len > 0u) {
    memcpy(&tx[12], fr->payload, fr->len);
  }
  crc = pat_crc32_ieee(tx, (size_t)(12u + fr->len));
  tx[12u + fr->len] = (uint8_t)(crc & 0xFFu);
  tx[12u + fr->len + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
  tx[12u + fr->len + 2u] = (uint8_t)((crc >> 16) & 0xFFu);
  tx[12u + fr->len + 3u] = (uint8_t)((crc >> 24) & 0xFFu);
  n = (uint16_t)(12u + fr->len + 4u);
  return HAL_UART_Transmit(huart, tx, n, 200u);
}
