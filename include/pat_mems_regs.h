#ifndef PAT_MEMS_REGS_H
#define PAT_MEMS_REGS_H

#include <stddef.h>
#include <stdint.h>

#define PAT_MEMS_MAGIC_U32 0x534D454Du

#define PAT_MEMS_CTRL_ARM      (1u << 0)
#define PAT_MEMS_CTRL_PUMP_RUN (1u << 1)
#define PAT_MEMS_CTRL_EN_REQ   (1u << 2)

#define PAT_MEMS_FC_HZ_DEFAULT              667u
#define PAT_MEMS_DAC_PUMP_PERIOD_MS_DEFAULT 1u

typedef struct pat_mems_reg_block {
  uint32_t magic;
  uint32_t ctrl;
  uint32_t fc_hz;
  uint32_t commit_seq;
  uint16_t dac[4];
  uint8_t rsvd[8];
} pat_mems_reg_block_t;

_Static_assert(sizeof(pat_mems_reg_block_t) == 32u, "pat_mems_reg_block_t must stay 32 bytes.");

void pat_mems_regs_init(pat_mems_reg_block_t *rb);
void pat_mems_regs_commit_dac4(pat_mems_reg_block_t *rb, const uint16_t dac[4]);
int pat_mems_regs_snapshot_dac4(const pat_mems_reg_block_t *rb, uint16_t dac_out[4], uint32_t *seq_out);
void pat_mems_reg_export(const pat_mems_reg_block_t *rb, void *dst, size_t n);

#endif
