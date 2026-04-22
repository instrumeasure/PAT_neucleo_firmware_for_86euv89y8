#ifndef QPD_DSP_H
#define QPD_DSP_H

#include <stdint.h>
#include "ads127l11.h"

#define QPD_RING_LEN 16U
#define QPD_SUM_SHIFT 4U

typedef struct
{
    int32_t y_raw[ADS127_SYNC_CHANNELS];
    int32_t y_i[ADS127_SYNC_CHANNELS];
    int32_t y_q[ADS127_SYNC_CHANNELS];
    uint8_t p_lo; /* LO phase nibble after step (SPI6 header byte 5) */
} qpd_dsp_output_t;

void qpd_dsp_init(void);
void qpd_dsp_set_step(uint8_t step);
void qpd_dsp_on_quartet(const ads127_sample_set_t *s, qpd_dsp_output_t *out);

#endif
