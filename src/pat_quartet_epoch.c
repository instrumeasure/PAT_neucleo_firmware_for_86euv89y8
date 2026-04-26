#include "pat_quartet_epoch.h"
#include "pat_quartet_p4_dma.h"
#include "stm32h7xx_hal.h"

int32_t pat_quartet_sign_extend_u24(uint32_t u24)
{
  return ads127_raw24_to_s32(u24);
}

void pat_quartet_epoch_line_publish(pat_quartet_epoch_line_t *line, const uint8_t raw24[ADS127_QUARTET_CHANNELS][3])
{
  if (line == 0) {
    return;
  }
  line->valid = 0u;
  __DSB();
  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    line->raw24[i][0] = raw24[i][0];
    line->raw24[i][1] = raw24[i][1];
    line->raw24[i][2] = raw24[i][2];
  }
  line->epoch_id = line->epoch_id + 1u;
  __DSB();
  line->valid = 1u;
}

void pat_quartet_epoch_line_invalidate(pat_quartet_epoch_line_t *line)
{
  if (line == 0) {
    return;
  }
  line->valid = 0u;
}
