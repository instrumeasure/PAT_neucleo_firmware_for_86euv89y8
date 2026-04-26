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

/** CONFIG3 FILTER[4:0] (SBAS946 Table 8-25): wideband, OSR 512 = 00100b. */
#define ADS127_CONFIG3_FILTER_WIDEBAND_OSR512 0x04u

#define ADS127_CMD_RREG(addr)   ((uint8_t)(0x40u | ((addr) & 0x0Fu)))
#define ADS127_CMD_WREG(addr)   ((uint8_t)(0x80u | ((addr) & 0x0Fu)))

#define ADS127_QUARTET_CHANNELS 4u

/*
 * ADS127L11 data words are 24-bit two's-complement, MSB first (SBAS946 serial output).
 * For a 3-byte sample, out24[0] is the sign byte on the wire.
 */
static inline int32_t ads127_raw24_to_s32(uint32_t u24)
{
  u24 &= 0xFFFFFFu;
  return (int32_t)(u24 << 8u) >> 8;
}

/**
 * **`pat_nucleo_quartet`:** CMake always sets **`PAT_QUARTET_PARALLEL_DRDY_WAIT=1`**. `ads127_read_quartet_blocking`
 * asserts all four !CS, waits for **SPI4 !DRDY on PE15** (duplicate MISO net) **before** any SCLK, then the 3-byte
 * sample: default **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER`** → `pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`
 * (interleaved register SPI); **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER=OFF`** → `HAL_SPI_TransmitReceive_IT` on
 * SPI1..SPI4. Other firmware images leave **`PAT_QUARTET_PARALLEL_DRDY_WAIT`** **0**; `ads127_read_quartet_blocking` then
 * returns **HAL_ERROR**.
 */
#ifndef PAT_QUARTET_PARALLEL_DRDY_WAIT
#define PAT_QUARTET_PARALLEL_DRDY_WAIT 0
#endif

/**
 * **1:** TI SBAS946 §8.5.9 **3-wire SPI**: each ADS127 !CS net is held **low** from GPIO init through **nRESET**
 * so the device latches 3-wire at POR (STATUS.CS_MODE = 1). The MCU **must not** drive !CS high or the part
 * returns to 4-wire. SCLK bit count delimits frames (no CS framing). CMake on `pat_nucleo_quartet` only.
 */
#ifndef PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
#define PAT_ADS127_SPI_3WIRE_CS_HELD_LOW 0
#endif

/** **1:** with `PAT_QUARTET_PARALLEL_DRDY_WAIT`: characterisation path — sequential `spi_master_rx3_zero_tx_unlocked` per bus (no parallel IT overlap). CMake `pat_nucleo_quartet` only. */
#ifndef PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED
#define PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED 0
#endif

/**
 * **1:** (with `PAT_QUARTET_PARALLEL_DRDY_WAIT`, and not `PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED`) sample phase uses
 * **`pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`** — no `HAL_SPI_TransmitReceive_*` / SPI NVIC for data.
 * **0:** bisect baseline — `HAL_SPI_TransmitReceive_IT` + `pat_quartet_spi_irq.c`. CMake `pat_nucleo_quartet` only.
 */
#ifndef PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER
#define PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER 0
#endif

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
 * wideband OSR512 CONFIG3 FILTER=00100b (`ADS127_CONFIG3_FILTER_WIDEBAND_OSR512`). PF1 held low during WREG to 04h–0Eh.
 *
 * `ads127_bringup`: pulses shared **nRESET** then programmes the bus (single-channel apps).
 * Quartet: one shared `ads127_nreset_pulse()` then **`ads127_bringup_no_nreset`** per SPI — calling
 * `ads127_bringup` on each channel would reset every ADC each time and only the last bus retains config.
 */
int ads127_bringup(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg);

/** Register programming only; no nRESET (use after one shared `ads127_nreset_pulse()` on quartet). */
int ads127_bringup_no_nreset(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg);

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
 * Returns 0 if OK; -1 shadow SPI error; -2 CONFIG4 CLK_SEL; -3 CONFIG3 filter (expect OSR512 code); -4 CONFIG2 SDO_MODE;
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
 * with SPI (SPE) off. MISO is briefly **GPIO input** (fast `MODER` plus pull-up); wait uses **DWT**
 * cycle budget (no `HAL_GetTick` in the inner loop). Phase B: **AF** + **H7 SPI v2** 3-byte path without
 * `HAL_SPI_TransmitReceive` lock (same TXP or RXP plus EOT behaviour as HAL), then `!CS` high.
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
 * One epoch ( **`PAT_QUARTET_PARALLEL_DRDY_WAIT=1`** on `pat_nucleo_quartet` ): shared !CS, **SPI4 PE15** !DRDY
 * gate, then register quartet SPI (default) or HAL SPI IT. Returns **HAL_ERROR** if this translation unit was built
 * without quartet parallel support (`PAT_QUARTET_PARALLEL_DRDY_WAIT=0`).
 *
 * **Partial epoch:** on fail at `i`, `< i` valid, `> i` **0xFF**; parallel timeout / SPI fault paths clear similarly.
 */
HAL_StatusTypeDef ads127_read_quartet_blocking(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint8_t out24[ADS127_QUARTET_CHANNELS][3],
    uint32_t timeout_ms,
    ads127_diag_t dg[ADS127_QUARTET_CHANNELS]);

/** Successful full quartet epochs (all four `HAL_OK`) since boot; for telemetry / AGENTS heartbeat. */
uint32_t ads127_get_quartet_acquired_count(void);

#endif
