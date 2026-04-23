#ifndef ADS127L11_H
#define ADS127L11_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define ADS127_REG_DEV_ID   0x00u
#define ADS127_REG_REV_ID   0x01u
#define ADS127_REG_STATUS   0x02u
#define ADS127_REG_CONTROL  0x03u
#define ADS127_REG_MUX      0x04u
#define ADS127_REG_CONFIG1  0x05u
#define ADS127_REG_CONFIG2  0x06u
#define ADS127_REG_CONFIG3  0x07u
#define ADS127_REG_CONFIG4  0x08u

#define ADS127_CMD_RREG(addr)   ((uint8_t)(0x40u | ((addr) & 0x0Fu)))
#define ADS127_CMD_WREG(addr)   ((uint8_t)(0x80u | ((addr) & 0x0Fu)))

#define ADS127_QUARTET_CHANNELS 4u

/** Per-channel SPI + soft !CS + MISO (DRDY poll) for one ADS127L11. */
typedef struct {
  SPI_HandleTypeDef *hspi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  GPIO_TypeDef *miso_port;
  uint16_t miso_pin;
  /** MISO AF number for `HAL_GPIO_Init` after brief `GPIO_MODE_INPUT` DRDY poll (H7 / SPI4). */
  uint32_t miso_af;
} ads127_ch_ctx_t;

/** Shadow 00h–08h (SBAS946 Table 8-16 subset). */
typedef struct {
  uint8_t dev_id;
  uint8_t rev_id;
  uint8_t status;
  uint8_t control;
  uint8_t mux;
  uint8_t config1;
  uint8_t config2;
  uint8_t config3;
  uint8_t config4;
} ads127_shadow_t;

typedef struct {
  uint32_t fault_mask;
  uint32_t drdy_timeouts;
  uint32_t last_sample_u32_be;
  /** Set if MISO (SDO/DRDY) was already low when the DRDY wait started (line already asserted ready). */
  uint8_t drdy_skipped_arm_high;
} ads127_diag_t;

void ads127_pins_init(void);
/** Hold !CS (PE11) low for ms_low then release — slow edge for LA / DMM (no SPI clocks). */
void ads127_cs_probe_pulse_ms(uint32_t ms_low);
void ads127_nreset_pulse(void);
void ads127_start_set(int run);

/**
 * Milliseconds to wait after `ads127_start_set(1)` before post-START RREG / first sample
 * (conversion + filter + SDO settle; SPI3 benefits from ≥20 ms).
 */
#define ADS127_START_STREAM_SETTLE_MS 25u

/** Bind logical channel index 0..3 (SPI1..SPI4 per AGENTS order) to an initialised `hspi`. */
void ads127_ch_ctx_bind(ads127_ch_ctx_t *ctx, unsigned ch_index, SPI_HandleTypeDef *hspi);

HAL_StatusTypeDef ads127_rreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t *out);
HAL_StatusTypeDef ads127_wreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t data);

HAL_StatusTypeDef ads127_shadow_refresh(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh);

/**
 * Milestone 1 bring-up: external CLK (CONFIG4 bit7), SDO_MODE+START_MODE in CONFIG2,
 * wideband OSR256 CONFIG3=0x03. PF1 held low during WREG to 04h–0Eh.
 */
int ads127_bringup(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg);

/** Returns 1 if `bringup_err == 0` and `fault_mask == 0` (safe to stream); else 0. */
int ads127_bringup_ok(int bringup_err, uint32_t fault_mask);

/** UART: decode `ads127_diag_t::fault_mask` bit meanings (no-op if mask is 0). */
void ads127_print_fault_mask(uint32_t fault_mask);

/**
 * Run `ads127_bringup` up to `attempts` times, issuing `ads127_nreset_pulse()` + delay before
 * attempts after the first. Returns last `ads127_bringup` return value.
 */
int ads127_bringup_retry(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg, unsigned attempts);

/**
 * After `ads127_start_set(1)` and `ADS127_START_STREAM_SETTLE_MS`: verify programmed mode via shadow refresh.
 * Briefly asserts **START low** so RREG readback is not corrupted by SDO/DRDY activity (SPI3).
 * Returns 0 if OK; -1 shadow SPI error; -2 CONFIG4 CLK_SEL; -3 CONFIG3 filter; -4 CONFIG2 SDO_MODE;
 * -5 shadow all-zero (suspect float / no readback).
 */
int ads127_post_start_gate(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh);

/**
 * Non-strict recovery: call when `ads127_post_start_gate` failed but firmware will stream anyway.
 * Pulses **START** low briefly then high and waits `ADS127_START_STREAM_SETTLE_MS` so conversions
 * and SDO/DRDY resume after the gate’s START-low RREG sequence (SPI3).
 */
void ads127_after_failed_post_start_gate(void);

/** `ads127_start_set(0)` then slow LED blink + blocking loop (call after UART/LED init). */
void ads127_halt_streaming_fault(const char *msg);

/**
 * Phase A: !CS low, **≥100 ns** (`delay_after_cs_100ns`), then poll MISO until **SDO/DRDY low** (ready),
 * with SPI (SPE) off. MISO is briefly **GPIO input** with **pull-up** so **IDR** tracks the pad, then AF
 * for the 24 SCLKs. Phase B: 24-bit read (MOSI 0x00,0x00,0x00).
 */
HAL_StatusTypeDef ads127_read_sample24_blocking(
    SPI_HandleTypeDef *hspi,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg);

/** Same as ads127_read_sample24_blocking for an explicit channel context (quartet / tests). */
HAL_StatusTypeDef ads127_read_sample24_ch_blocking(
    const ads127_ch_ctx_t *ch,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg);

/**
 * One epoch: SPI1→SPI2→SPI3→SPI4 per AGENTS. Fills out24[ch][3]; updates dg[ch] per channel.
 * Returns first non-HAL_OK status, or HAL_OK if all four succeed.
 *
 * **Partial epoch contract:** On non-HAL_OK return at channel index `i`, indices `< i` contain
 * this attempt’s data; indices `> i` are set to **0xFF** in all three bytes; `dg[k]` for `k > i`
 * are zero-cleared. Channel `i` may contain partial data depending where failure occurred inside
 * `ads127_read_sample24_ch_blocking`.
 */
HAL_StatusTypeDef ads127_read_quartet_blocking(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint8_t out24[ADS127_QUARTET_CHANNELS][3],
    uint32_t timeout_ms,
    ads127_diag_t dg[ADS127_QUARTET_CHANNELS]);

/** Successful full quartet epochs (all four `HAL_OK`) since boot; for telemetry / AGENTS heartbeat. */
uint32_t ads127_get_quartet_acquired_count(void);

#endif
