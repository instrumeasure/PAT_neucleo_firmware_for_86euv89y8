#ifndef AD5664R_H
#define AD5664R_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

typedef struct ad5664r_dev {
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
} ad5664r_dev_t;

void ad5664r_init_dev(ad5664r_dev_t *dev, SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin);
HAL_StatusTypeDef ad5664r_write_raw24(ad5664r_dev_t *dev, uint32_t raw24);
HAL_StatusTypeDef ad5664r_write_channel_u16(ad5664r_dev_t *dev, uint8_t channel, uint16_t code);
HAL_StatusTypeDef ad5664r_init_sequence(ad5664r_dev_t *dev, uint16_t mid_scale);

#endif
