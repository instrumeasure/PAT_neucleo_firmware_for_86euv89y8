#ifndef PAT_QUARTET_APP_H
#define PAT_QUARTET_APP_H

#include "stm32h7xx_hal.h"
#include "ads127l11.h"

/** UART summary period for `EPOCH`/`CH`/`CNT` lines (milliseconds); not ADC ODR. */
#ifndef PAT_QUARTET_SYNC_SUMMARY_MS
#define PAT_QUARTET_SYNC_SUMMARY_MS 1000u
#endif

/** First N epochs print every-epoch `EPOCH`/`CH` if non-zero (define at compile time). */
#ifndef PAT_QUARTET_SYNC_BURST_EPOCHS
#define PAT_QUARTET_SYNC_BURST_EPOCHS 0u
#endif

/** CMake `PAT_QUARTET_DIAG_EPOCH_EVERY=ON`: print `CNT`/`EPOCH`/`CH` every epoch (UART flood; LA correlate `span_us`). */
#ifndef PAT_QUARTET_DIAG_EPOCH_EVERY
#define PAT_QUARTET_DIAG_EPOCH_EVERY 0
#endif

void pat_quartet_app_print_sync_debug_boot(void);

/**
 * One shared **nRESET** per attempt, then **`ads127_bringup_no_nreset`** on SPI1..4 (avoids resetting all
 * ADCs between channels). Prints `BRU`/`SH`/`TI`/`STAT` per channel.
 * @return 1 if all channels `ads127_bringup_ok`, else 0.
 */
unsigned pat_quartet_app_bringup_retry_all(
    SPI_HandleTypeDef *hs[ADS127_QUARTET_CHANNELS],
    ads127_shadow_t sh[ADS127_QUARTET_CHANNELS],
    ads127_diag_t dg_bu[ADS127_QUARTET_CHANNELS],
    int br_ch[ADS127_QUARTET_CHANNELS]);

/** START high + settle, then post-START gate per channel (strict handled in caller via ifdef). */
void pat_quartet_app_post_start_gates_nonstrict(
    SPI_HandleTypeDef *hs[ADS127_QUARTET_CHANNELS],
    ads127_shadow_t sh[ADS127_QUARTET_CHANNELS],
    const int br_ch[ADS127_QUARTET_CHANNELS],
    const ads127_diag_t dg_bu[ADS127_QUARTET_CHANNELS]);

#endif
