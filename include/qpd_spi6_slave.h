#ifndef QPD_SPI6_SLAVE_H
#define QPD_SPI6_SLAVE_H

#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "ads127l11.h"
#include "qpd_dsp.h"

void qpd_spi6_slave_init(SPI_HandleTypeDef *hspi);
void qpd_spi6_slave_pack_latest(const ads127_sample_set_t *s, const qpd_dsp_output_t *dsp);
void qpd_spi6_slave_stage_frame64(const uint8_t frame64[64]);

#endif
