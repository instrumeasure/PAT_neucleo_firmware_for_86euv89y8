#ifndef PAT_QUARTET_ROLLING_H
#define PAT_QUARTET_ROLLING_H

#include <stddef.h>
#include <stdint.h>

#include "ads127l11.h"

#ifndef PAT_QUARTET_ROLL_LEN
#define PAT_QUARTET_ROLL_LEN 8u
#endif

#ifndef PAT_QUARTET_ROLL_SHIFT
#define PAT_QUARTET_ROLL_SHIFT 3u
#endif

#define PAT_QROLL_FMT_0x0A 0x0Au
#define PAT_QROLL_P64_NBYTES 64u

#define PAT_QROLL_FLAG_MEAN_VALID 0x01u

/*
 * 8-state sign LUT knobs (bit=1 means negate x for that phase).
 * Defaults are square-sign quadrature style and can be overridden at compile time.
 */
#ifndef PAT_QROLL_I_NEG_BITS
#define PAT_QROLL_I_NEG_BITS 0xF0u
#endif

#ifndef PAT_QROLL_Q_NEG_BITS
#define PAT_QROLL_Q_NEG_BITS 0x3Cu
#endif

/* Block 1 offsets (debug/export map; no public struct). */
#define PAT_ROLL_OFS_RAW 0u
#define PAT_ROLL_OFS_I 128u
#define PAT_ROLL_OFS_Q 256u
#define PAT_ROLL_OFS_ACC_RAW 384u
#define PAT_ROLL_OFS_ACC_I 400u
#define PAT_ROLL_OFS_ACC_Q 416u
#define PAT_ROLL_OFS_EPOCH_SEQ 432u
#define PAT_ROLL_OFS_WPOS 436u
#define PAT_ROLL_OFS_P 437u
#define PAT_ROLL_OFS_STEP 438u
#define PAT_ROLL_OFS_FLAGS 439u
#define PAT_ROLL_OFS_GOOD_EPOCHS 440u
#define PAT_ROLLING_STATE_NBYTES 448u

/* Block 2 payload offsets. */
#define PAT_QROLL_P64_OFS_EPOCH0 0u
#define PAT_QROLL_P64_OFS_FMT 4u
#define PAT_QROLL_P64_OFS_DDS_P 5u
#define PAT_QROLL_P64_OFS_FLAGS 6u
#define PAT_QROLL_P64_OFS_WPOS 7u
#define PAT_QROLL_P64_OFS_MEAN_RAW 8u
#define PAT_QROLL_P64_OFS_MEAN_I 24u
#define PAT_QROLL_P64_OFS_MEAN_Q 40u
#define PAT_QROLL_P64_OFS_RSVD 56u

void pat_quartet_rolling_init(void);
void pat_quartet_rolling_set_step(uint8_t step);
void pat_quartet_rolling_on_epoch(const uint8_t raw24[ADS127_QUARTET_CHANNELS][3]);
void pat_quartet_rolling_payload_fill_from_acc(void);

const uint8_t *pat_quartet_rolling_payload_read_slab(uint8_t *read_idx_out);
uint32_t pat_quartet_rolling_payload_epoch_seq(void);
uint8_t pat_quartet_rolling_flags(void);

size_t pat_quartet_rolling_state_size(void);
void pat_quartet_rolling_state_export(uint8_t *dst, size_t dst_nbytes);

#endif
