#ifndef PAT_QUARTET_EPOCH_H
#define PAT_QUARTET_EPOCH_H

#include <stdint.h>
#include "ads127l11.h"

/** 32-byte alignment for future DMA / DSP-friendly loads (H7 D-cache line size). */
#define PAT_QUARTET_EPOCH_ALIGN_BYTES 32u

/**
 * Published snapshot of one SPI1→SPI4 epoch (four 24-bit words + metadata).
 * Writer sets `raw24` then `epoch_id`, then `valid=1`. Consumer reads `valid` then data.
 */
typedef struct __attribute__((aligned(32))) pat_quartet_epoch_line {
  volatile uint32_t epoch_id;
  volatile uint8_t valid;
  uint8_t pad[3];
  uint8_t raw24[ADS127_QUARTET_CHANNELS][3];
} pat_quartet_epoch_line_t;

/** Sign-extend one 24-bit big-endian triplet to int32_t. */
int32_t pat_quartet_sign_extend_u24(uint32_t u24);

/**
 * Copy four raw24 arrays into line, bump epoch_id, set valid (release ordering).
 */
void pat_quartet_epoch_line_publish(pat_quartet_epoch_line_t *line, const uint8_t raw24[ADS127_QUARTET_CHANNELS][3]);

void pat_quartet_epoch_line_invalidate(pat_quartet_epoch_line_t *line);

#endif
