#include "pat_mems_regs.h"

#include <string.h>

void pat_mems_regs_init(pat_mems_reg_block_t *rb)
{
  if (rb == NULL) {
    return;
  }
  memset(rb, 0, sizeof(*rb));
  rb->magic = PAT_MEMS_MAGIC_U32;
  rb->fc_hz = PAT_MEMS_FC_HZ_DEFAULT;
  rb->dac[0] = 0x8000u;
  rb->dac[1] = 0x8000u;
  rb->dac[2] = 0x8000u;
  rb->dac[3] = 0x8000u;
  rb->commit_seq = 0u;
}

void pat_mems_regs_commit_dac4(pat_mems_reg_block_t *rb, const uint16_t dac[4])
{
  if ((rb == NULL) || (dac == NULL)) {
    return;
  }
  rb->dac[0] = dac[0];
  rb->dac[1] = dac[1];
  rb->dac[2] = dac[2];
  rb->dac[3] = dac[3];
  rb->commit_seq++;
}

int pat_mems_regs_snapshot_dac4(const pat_mems_reg_block_t *rb, uint16_t dac_out[4], uint32_t *seq_out)
{
  uint32_t s0;
  uint32_t s1;

  if ((rb == NULL) || (dac_out == NULL)) {
    return -1;
  }

  s0 = rb->commit_seq;
  dac_out[0] = rb->dac[0];
  dac_out[1] = rb->dac[1];
  dac_out[2] = rb->dac[2];
  dac_out[3] = rb->dac[3];
  s1 = rb->commit_seq;
  if (s0 != s1) {
    return -2;
  }
  if (seq_out != NULL) {
    *seq_out = s1;
  }
  return 0;
}

void pat_mems_reg_export(const pat_mems_reg_block_t *rb, void *dst, size_t n)
{
  size_t c;

  if ((rb == NULL) || (dst == NULL) || (n == 0u)) {
    return;
  }
  c = (n < sizeof(*rb)) ? n : sizeof(*rb);
  memcpy(dst, rb, c);
}
