#include "pat_quartet_rolling.h"

#include <string.h>

static int32_t g_roll_raw[ADS127_QUARTET_CHANNELS][PAT_QUARTET_ROLL_LEN];
static int32_t g_roll_i[ADS127_QUARTET_CHANNELS][PAT_QUARTET_ROLL_LEN];
static int32_t g_roll_q[ADS127_QUARTET_CHANNELS][PAT_QUARTET_ROLL_LEN];
static int32_t g_acc_raw[ADS127_QUARTET_CHANNELS];
static int32_t g_acc_i[ADS127_QUARTET_CHANNELS];
static int32_t g_acc_q[ADS127_QUARTET_CHANNELS];

static uint32_t g_epoch_seq;
static uint32_t g_good_epochs;
static uint8_t g_wpos;
static uint8_t g_p;
static uint8_t g_step;
static uint8_t g_flags;

static uint8_t g_p64[2][PAT_QROLL_P64_NBYTES];
static uint8_t g_read_idx;

_Static_assert(PAT_QUARTET_ROLL_LEN == 8u, "PAT_QUARTET_ROLL_LEN must stay 8.");
_Static_assert(PAT_QUARTET_ROLL_SHIFT == 3u, "PAT_QUARTET_ROLL_SHIFT must stay 3.");
_Static_assert(sizeof(g_roll_raw) + sizeof(g_roll_i) + sizeof(g_roll_q) + sizeof(g_acc_raw) +
                   sizeof(g_acc_i) + sizeof(g_acc_q) + 16u == PAT_ROLLING_STATE_NBYTES,
               "PAT_ROLLING_STATE_NBYTES mismatch.");
_Static_assert(sizeof(g_p64) == 128u, "Block 2 slab storage must be 128 bytes.");
_Static_assert(PAT_QROLL_P64_OFS_RSVD + 8u == PAT_QROLL_P64_NBYTES, "Payload offsets must sum to 64 bytes.");

static inline void put_le32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static inline uint8_t qroll_neg_i(uint8_t p3)
{
  return (uint8_t)((PAT_QROLL_I_NEG_BITS >> (p3 & 7u)) & 1u);
}

static inline uint8_t qroll_neg_q(uint8_t p3)
{
  return (uint8_t)((PAT_QROLL_Q_NEG_BITS >> (p3 & 7u)) & 1u);
}

void pat_quartet_rolling_init(void)
{
  memset(g_roll_raw, 0, sizeof(g_roll_raw));
  memset(g_roll_i, 0, sizeof(g_roll_i));
  memset(g_roll_q, 0, sizeof(g_roll_q));
  memset(g_acc_raw, 0, sizeof(g_acc_raw));
  memset(g_acc_i, 0, sizeof(g_acc_i));
  memset(g_acc_q, 0, sizeof(g_acc_q));
  memset(g_p64, 0, sizeof(g_p64));

  g_epoch_seq = 0u;
  g_good_epochs = 0u;
  g_wpos = 0u;
  g_p = 0u;
  g_step = 1u;
  g_flags = 0u;
  g_read_idx = 0u;
}

void pat_quartet_rolling_set_step(uint8_t step)
{
  g_step = (step == 0u) ? 1u : (uint8_t)(step & 7u);
  if (g_step == 0u) {
    g_step = 1u;
  }
}

void pat_quartet_rolling_on_epoch(const uint8_t raw24[ADS127_QUARTET_CHANNELS][3])
{
  const uint8_t slot = (uint8_t)(g_wpos & 7u);
  const uint8_t p3 = (uint8_t)(g_p & 7u);
  const uint8_t ni = qroll_neg_i(p3);
  const uint8_t nq = qroll_neg_q(p3);

  for (uint8_t ch = 0u; ch < ADS127_QUARTET_CHANNELS; ch++) {
    const uint32_t u24 = ((uint32_t)raw24[ch][0] << 16) | ((uint32_t)raw24[ch][1] << 8) | (uint32_t)raw24[ch][2];
    const int32_t x = ads127_raw24_to_s32(u24);
    const int32_t vi = (ni != 0u) ? -x : x;
    const int32_t vq = (nq != 0u) ? -x : x;

    const int32_t evr = g_roll_raw[ch][slot];
    const int32_t evi = g_roll_i[ch][slot];
    const int32_t evq = g_roll_q[ch][slot];

    g_roll_raw[ch][slot] = x;
    g_roll_i[ch][slot] = vi;
    g_roll_q[ch][slot] = vq;

    g_acc_raw[ch] += (x - evr);
    g_acc_i[ch] += (vi - evi);
    g_acc_q[ch] += (vq - evq);
  }

  g_wpos = (uint8_t)((slot + 1u) & 7u);
  g_p = (uint8_t)((g_p + g_step) & 7u);
  if (g_good_epochs < 0xFFFFFFFFu) {
    g_good_epochs++;
  }
  if (g_good_epochs >= PAT_QUARTET_ROLL_LEN) {
    g_flags |= PAT_QROLL_FLAG_MEAN_VALID;
  }
}

void pat_quartet_rolling_payload_fill_from_acc(void)
{
  const uint8_t wi = (uint8_t)(g_read_idx ^ 1u);
  uint8_t *const f = &g_p64[wi][0];

  g_epoch_seq++;
  put_le32(&f[PAT_QROLL_P64_OFS_EPOCH0], g_epoch_seq);
  f[PAT_QROLL_P64_OFS_FMT] = PAT_QROLL_FMT_0x0A;
  f[PAT_QROLL_P64_OFS_DDS_P] = (uint8_t)(g_p & 7u);
  f[PAT_QROLL_P64_OFS_FLAGS] = g_flags;
  f[PAT_QROLL_P64_OFS_WPOS] = (uint8_t)(g_wpos & 7u);

  for (uint8_t ch = 0u; ch < ADS127_QUARTET_CHANNELS; ch++) {
    put_le32(&f[PAT_QROLL_P64_OFS_MEAN_RAW + (4u * ch)], (uint32_t)(g_acc_raw[ch] >> PAT_QUARTET_ROLL_SHIFT));
    put_le32(&f[PAT_QROLL_P64_OFS_MEAN_I + (4u * ch)], (uint32_t)(g_acc_i[ch] >> PAT_QUARTET_ROLL_SHIFT));
    put_le32(&f[PAT_QROLL_P64_OFS_MEAN_Q + (4u * ch)], (uint32_t)(g_acc_q[ch] >> PAT_QUARTET_ROLL_SHIFT));
  }
  memset(&f[PAT_QROLL_P64_OFS_RSVD], 0, 8u);

  __DSB();
  g_read_idx = wi;
  __DSB();
}

const uint8_t *pat_quartet_rolling_payload_read_slab(uint8_t *read_idx_out)
{
  const uint8_t idx = g_read_idx;
  if (read_idx_out != NULL) {
    *read_idx_out = idx;
  }
  return &g_p64[idx][0];
}

uint32_t pat_quartet_rolling_payload_epoch_seq(void)
{
  return g_epoch_seq;
}

uint8_t pat_quartet_rolling_flags(void)
{
  return g_flags;
}

size_t pat_quartet_rolling_state_size(void)
{
  return PAT_ROLLING_STATE_NBYTES;
}

void pat_quartet_rolling_state_export(uint8_t *dst, size_t dst_nbytes)
{
  if (dst == NULL || dst_nbytes < PAT_ROLLING_STATE_NBYTES) {
    return;
  }

  memset(dst, 0, PAT_ROLLING_STATE_NBYTES);
  memcpy(&dst[PAT_ROLL_OFS_RAW], g_roll_raw, sizeof(g_roll_raw));
  memcpy(&dst[PAT_ROLL_OFS_I], g_roll_i, sizeof(g_roll_i));
  memcpy(&dst[PAT_ROLL_OFS_Q], g_roll_q, sizeof(g_roll_q));
  memcpy(&dst[PAT_ROLL_OFS_ACC_RAW], g_acc_raw, sizeof(g_acc_raw));
  memcpy(&dst[PAT_ROLL_OFS_ACC_I], g_acc_i, sizeof(g_acc_i));
  memcpy(&dst[PAT_ROLL_OFS_ACC_Q], g_acc_q, sizeof(g_acc_q));
  put_le32(&dst[PAT_ROLL_OFS_EPOCH_SEQ], g_epoch_seq);
  dst[PAT_ROLL_OFS_WPOS] = g_wpos;
  dst[PAT_ROLL_OFS_P] = g_p;
  dst[PAT_ROLL_OFS_STEP] = g_step;
  dst[PAT_ROLL_OFS_FLAGS] = g_flags;
  put_le32(&dst[PAT_ROLL_OFS_GOOD_EPOCHS], g_good_epochs);
}
