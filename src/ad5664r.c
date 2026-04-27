#include "ad5664r.h"

static HAL_StatusTypeDef ad5664r_tx3(ad5664r_dev_t *dev, const uint8_t tx[3])
{
  HAL_StatusTypeDef st;
  if ((dev == NULL) || (dev->hspi == NULL) || (dev->cs_port == NULL) || (tx == NULL)) {
    return HAL_ERROR;
  }
  HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
  st = HAL_SPI_Transmit(dev->hspi, (uint8_t *)tx, 3u, 100u);
  HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
  return st;
}

void ad5664r_init_dev(ad5664r_dev_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
  if (dev == NULL) {
    return;
  }
  dev->hspi = hspi;
  dev->cs_port = cs_port;
  dev->cs_pin = cs_pin;
}

HAL_StatusTypeDef ad5664r_write_raw24(ad5664r_dev_t *dev, uint32_t raw24)
{
  uint8_t tx[3];
  tx[0] = (uint8_t)((raw24 >> 16) & 0xFFu);
  tx[1] = (uint8_t)((raw24 >> 8) & 0xFFu);
  tx[2] = (uint8_t)(raw24 & 0xFFu);
  return ad5664r_tx3(dev, tx);
}

HAL_StatusTypeDef ad5664r_write_channel_u16(ad5664r_dev_t *dev, uint8_t channel, uint16_t code)
{
  /* AD5664R frame: [C3:C0 A3:A0 D15..D12][D11..D4][D3..D0 xxxx]
   * Use WRITE+UPDATE command (0x3), channel in low nibble. */
  uint32_t cmd;
  cmd = ((uint32_t)0x3u << 20) | (((uint32_t)channel & 0x0Fu) << 16) | ((uint32_t)code << 4);
  return ad5664r_write_raw24(dev, cmd);
}

HAL_StatusTypeDef ad5664r_init_sequence(ad5664r_dev_t *dev, uint16_t mid_scale)
{
  uint8_t ch;
  HAL_StatusTypeDef st = HAL_OK;
  for (ch = 0u; ch < 4u; ch++) {
    st = ad5664r_write_channel_u16(dev, ch, mid_scale);
    if (st != HAL_OK) {
      return st;
    }
  }
  return st;
}
