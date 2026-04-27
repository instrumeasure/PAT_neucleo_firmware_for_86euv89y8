#ifndef PAT_UART5_PAT5_H
#define PAT_UART5_PAT5_H

#include "stm32h7xx_hal.h"
#include <stddef.h>
#include <stdint.h>

#define PAT5_MAX_PAYLOAD 504u
#define PAT5_MAGIC0 0x50u
#define PAT5_MAGIC1 0x41u
#define PAT5_MAGIC2 0x54u
#define PAT5_MAGIC3 0x35u

typedef struct pat5_frame {
  uint8_t ver;
  uint8_t flags;
  uint16_t seq;
  uint16_t cmd;
  uint16_t len;
  uint8_t payload[PAT5_MAX_PAYLOAD];
} pat5_frame_t;

typedef struct pat_uart5_rx {
  uint8_t buf[1024];
  uint16_t head;
  uint16_t tail;
  uint32_t crc_fail;
  uint32_t len_fail;
} pat_uart5_rx_t;

void pat_uart5_rx_init(pat_uart5_rx_t *rx);
void pat_uart5_rx_push(pat_uart5_rx_t *rx, const uint8_t *src, uint16_t n);
int pat_uart5_try_parse(pat_uart5_rx_t *rx, pat5_frame_t *out);
HAL_StatusTypeDef pat_uart5_send(UART_HandleTypeDef *huart, const pat5_frame_t *fr);

#endif
