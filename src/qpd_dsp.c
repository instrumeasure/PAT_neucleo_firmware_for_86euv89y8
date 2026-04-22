#include "qpd_dsp.h"
#include <string.h>

/* 4-phase sign: idx = (p >> 2) & 3 -> +1,+1,-1,-1 (plan 6.4.3). */
static const int8_t sgn4[4] = {1, 1, -1, -1};

static int32_t ring_raw[ADS127_SYNC_CHANNELS][QPD_RING_LEN];
static int32_t ring_i[ADS127_SYNC_CHANNELS][QPD_RING_LEN];
static int32_t ring_q[ADS127_SYNC_CHANNELS][QPD_RING_LEN];

static int64_t acc_raw[ADS127_SYNC_CHANNELS];
static int64_t acc_i[ADS127_SYNC_CHANNELS];
static int64_t acc_q[ADS127_SYNC_CHANNELS];

static uint8_t wpos;
static uint8_t p;
static uint8_t step = 1U;

static int32_t sign_extend_24(int32_t v)
{
    v &= 0x00FFFFFFL;
    if (v & 0x00800000L)
    {
        v |= 0xFF000000L;
    }
    return v;
}

void qpd_dsp_init(void)
{
    memset(ring_raw, 0, sizeof(ring_raw));
    memset(ring_i, 0, sizeof(ring_i));
    memset(ring_q, 0, sizeof(ring_q));
    memset(acc_raw, 0, sizeof(acc_raw));
    memset(acc_i, 0, sizeof(acc_i));
    memset(acc_q, 0, sizeof(acc_q));
    wpos = 0U;
    p = 0U;
    step = 1U;
}

void qpd_dsp_set_step(uint8_t s)
{
    step = (s == 0U) ? 1U : s;
}

void qpd_dsp_on_quartet(const ads127_sample_set_t *s, qpd_dsp_output_t *out)
{
    int32_t x;
    int32_t evr, evi, evq;
    int8_t si, sq;
    uint8_t ch;
    uint8_t p_use;
    uint8_t idx_i;
    uint8_t idx_q;

    if (s == NULL || out == NULL)
    {
        return;
    }

    p_use = p & 0x0FU;
    idx_i = (uint8_t)((p_use >> 2) & 3U);
    idx_q = (uint8_t)((((p_use + 4U) & 0x0FU) >> 2) & 3U);
    si = sgn4[idx_i];
    sq = sgn4[idx_q];

    for (ch = 0U; ch < ADS127_SYNC_CHANNELS; ch++)
    {
        if (s->raw[ch] == ADS127_RAW_INVALID)
        {
            x = 0;
        }
        else
        {
            x = sign_extend_24(s->raw[ch]);
        }

        evr = ring_raw[ch][wpos];
        evi = ring_i[ch][wpos];
        evq = ring_q[ch][wpos];

        ring_raw[ch][wpos] = x;
        ring_i[ch][wpos] = (int32_t)((int32_t)x * (int32_t)si);
        ring_q[ch][wpos] = (int32_t)((int32_t)x * (int32_t)sq);

        acc_raw[ch] += (int64_t)ring_raw[ch][wpos] - (int64_t)evr;
        acc_i[ch] += (int64_t)ring_i[ch][wpos] - (int64_t)evi;
        acc_q[ch] += (int64_t)ring_q[ch][wpos] - (int64_t)evq;

        out->y_raw[ch] = (int32_t)(acc_raw[ch] >> QPD_SUM_SHIFT);
        out->y_i[ch] = (int32_t)(acc_i[ch] >> QPD_SUM_SHIFT);
        out->y_q[ch] = (int32_t)(acc_q[ch] >> QPD_SUM_SHIFT);
    }

    wpos = (uint8_t)((wpos + 1U) & (QPD_RING_LEN - 1U));
    p = (uint8_t)((p + step) & 0x0FU);
    out->p_lo = p & 0x0FU;
}
