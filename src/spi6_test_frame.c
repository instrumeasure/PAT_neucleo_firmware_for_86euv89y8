#include "spi6_test_frame.h"
#include <string.h>

void spi6_test_frame_fill(uint8_t tx[SPI6_TEST_FRAME_N], const uint8_t *rx, uint32_t completed_frame_index)
{
  if (completed_frame_index == SPI6_TEST_FRAME_INDEX_IDLE) {
    tx[0] = 0xFFU;
    tx[1] = 0xFFU;
    tx[2] = 0xFFU;
    tx[3] = 0xFFU;
    tx[4] = 0xA5U;
    (void)memset(&tx[5], 0, (size_t)(SPI6_TEST_FRAME_N - 5U));
    return;
  }

  uint32_t seq = completed_frame_index + 1U;
  tx[0] = (uint8_t)(seq & 0xFFU);
  tx[1] = (uint8_t)((seq >> 8) & 0xFFU);
  tx[2] = (uint8_t)((seq >> 16) & 0xFFU);
  tx[3] = (uint8_t)((seq >> 24) & 0xFFU);
  tx[4] = 0xA5U;

  if (rx != NULL) {
    tx[5] = rx[0];
    tx[6] = rx[1];
    tx[7] = rx[2];
  } else {
    tx[5] = 0U;
    tx[6] = 0U;
    tx[7] = 0U;
  }

  for (uint32_t i = 8U; i < SPI6_TEST_FRAME_N; i++) {
    tx[i] = (uint8_t)(seq ^ (uint8_t)(i * 0x1DU));
  }
}
