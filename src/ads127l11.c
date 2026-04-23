#include "ads127l11.h"
#include "pat_pinmap.h"

/* Logical channel 3 — must match PAT_PINMAP_SPI4_* */
#define PORT_CS    PAT_PINMAP_SPI4_NCS_PORT
#define PIN_CS     PAT_PINMAP_SPI4_NCS_PIN
#define PORT_RST   PAT_PINMAP_ADS127_NRESET_PORT
#define PIN_RST    PAT_PINMAP_ADS127_NRESET_PIN
#define PORT_START PAT_PINMAP_ADS127_START_PORT
#define PIN_START  PAT_PINMAP_ADS127_START_PIN
#define PORT_MISO_LINE PAT_PINMAP_SPI4_MISO_PORT
#define PIN_MISO       PAT_PINMAP_SPI4_MISO_PIN

/* SBAS946 td(RSSC) ≥ 10000·t_CLK @ 25 MHz mod CLK → 400 µs; use margin for first SPI. */
#define TD_RSSC_MS 5u
#define RREG_INTER_FRAME_MS 2u
#define POST_WREG_MS 2u

static void cs_high(void)
{
  HAL_GPIO_WritePin(PORT_CS, PIN_CS, GPIO_PIN_SET);
}

static void cs_low(void)
{
  HAL_GPIO_WritePin(PORT_CS, PIN_CS, GPIO_PIN_RESET);
}

static void delay_short(void)
{
  for (volatile uint32_t i = 0; i < 200u; i++) {
    __NOP();
  }
}

/* ≥100 ns after !CS before sampling SDO/DRDY; ~48× __NOP @ ≥400 MHz is nominally ≥100 ns. */
static void delay_after_cs_100ns(void)
{
  for (volatile uint32_t i = 0; i < 48u; i++) {
    __NOP();
  }
}

static inline uint32_t miso_line_high_raw(void)
{
  return (PORT_MISO_LINE->IDR & (uint32_t)PIN_MISO) != 0u ? 1u : 0u;
}

/* Only call HAL_GetTick every (mask+1) polls — tick dominates loop time vs IDR read. */
#define MISO_POLL_TICK_MASK 0x1FFu

void ads127_pins_init(void)
{
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin = PIN_CS;
  HAL_GPIO_Init(PORT_CS, &g);
  cs_high();

  g.Pin = PIN_RST | PIN_START;
  HAL_GPIO_Init(PORT_RST, &g);
  HAL_GPIO_WritePin(PORT_RST, PIN_RST, GPIO_PIN_SET);
  HAL_GPIO_WritePin(PORT_START, PIN_START, GPIO_PIN_RESET);
}

void ads127_cs_probe_pulse_ms(uint32_t ms_low)
{
  cs_high();
  HAL_Delay(2u);
  cs_low();
  if (ms_low != 0u) {
    HAL_Delay(ms_low);
  }
  cs_high();
}

void ads127_nreset_pulse(void)
{
  HAL_GPIO_WritePin(PORT_RST, PIN_RST, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(PORT_RST, PIN_RST, GPIO_PIN_SET);
  HAL_Delay(TD_RSSC_MS);
}

void ads127_start_set(int run)
{
  HAL_GPIO_WritePin(PORT_START, PIN_START, run ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static HAL_StatusTypeDef spi_x(SPI_HandleTypeDef *hspi, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  cs_low();
  delay_short();
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(hspi, (uint8_t *)tx, rx, len, 200u);
  delay_short();
  cs_high();
  return st;
}

HAL_StatusTypeDef ads127_rreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t *out)
{
  uint8_t tx1[2] = { ADS127_CMD_RREG(addr), 0x00u };
  uint8_t rx1[2];
  HAL_StatusTypeDef st = spi_x(hspi, tx1, rx1, 2u);
  if (st != HAL_OK) {
    return st;
  }
  (void)rx1;
  /* Off-frame RREG: CS must go high between command and data frames; give the ADC time to decode. */
  HAL_Delay(RREG_INTER_FRAME_MS);

  uint8_t tx2[2] = { 0x00u, 0x00u };
  uint8_t rx2[2];
  st = spi_x(hspi, tx2, rx2, 2u);
  if (st != HAL_OK) {
    return st;
  }
  *out = rx2[0];
  return HAL_OK;
}

HAL_StatusTypeDef ads127_wreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t data)
{
  uint8_t tx[2] = { ADS127_CMD_WREG(addr), data };
  uint8_t rx[2];
  HAL_StatusTypeDef st = spi_x(hspi, tx, rx, 2u);
  if (st == HAL_OK) {
    HAL_Delay(POST_WREG_MS);
  }
  return st;
}

HAL_StatusTypeDef ads127_shadow_refresh(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh)
{
  HAL_StatusTypeDef st;
  st = ads127_rreg(hspi, ADS127_REG_DEV_ID, &sh->dev_id);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_REV_ID, &sh->rev_id);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_STATUS, &sh->status);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_CONTROL, &sh->control);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_MUX, &sh->mux);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_CONFIG1, &sh->config1);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_CONFIG2, &sh->config2);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_CONFIG3, &sh->config3);
  if (st != HAL_OK) {
    return st;
  }
  st = ads127_rreg(hspi, ADS127_REG_CONFIG4, &sh->config4);
  return st;
}

int ads127_bringup(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg)
{
  int err = 0;
  dg->fault_mask = 0u;

  ads127_nreset_pulse();

  uint8_t v;
  if (ads127_rreg(hspi, ADS127_REG_DEV_ID, &v) != HAL_OK) {
    dg->fault_mask |= 1u << 0;
    return -1;
  }
  /* 00h == ADS127L11 is valid; floating MISO also reads 00h — do not treat !=0 as only error. */
  if (v != 0x00u) {
    dg->fault_mask |= 1u << 1;
    err = -2;
  }

  uint8_t c4;
  if (ads127_rreg(hspi, ADS127_REG_CONFIG4, &c4) != HAL_OK) {
    dg->fault_mask |= 1u << 2;
    return -3;
  }
  /* External modulator CLK: CLK_SEL=1. Keep bits 3:0 PAT-default (24b, CRC off). */
  uint8_t c4_new = (uint8_t)((c4 | 0x80u) & 0xF0u);
  if (c4_new != c4) {
    ads127_start_set(0);
    if (ads127_wreg(hspi, ADS127_REG_CONFIG4, c4_new) != HAL_OK) {
      dg->fault_mask |= 1u << 3;
      err = -4;
    } else {
      uint8_t c4_rb = 0;
      if (ads127_rreg(hspi, ADS127_REG_CONFIG4, &c4_rb) != HAL_OK) {
        dg->fault_mask |= 1u << 9;
        err = -4;
      } else if ((c4_rb & 0x80u) == 0u) {
        /* No CLK_SEL readback — bus open / wrong CS / no device. */
        dg->fault_mask |= 1u << 10;
        err = -4;
      }
    }
  }

  uint8_t c2;
  if (ads127_rreg(hspi, ADS127_REG_CONFIG2, &c2) != HAL_OK) {
    dg->fault_mask |= 1u << 4;
    return -5;
  }
  /* SDO_MODE=1 (bit5), START_MODE=00 (bits4:3). */
  uint8_t c2_new = (uint8_t)((c2 & (uint8_t)~0x18u) | 0x20u);
  ads127_start_set(0);
  if (ads127_wreg(hspi, ADS127_REG_CONFIG2, c2_new) != HAL_OK) {
    dg->fault_mask |= 1u << 5;
    err = -6;
  } else {
    uint8_t c2_rb = 0;
    if (ads127_rreg(hspi, ADS127_REG_CONFIG2, &c2_rb) == HAL_OK && ((c2_rb & 0x20u) == 0u)) {
      dg->fault_mask |= 1u << 11;
      err = -6;
    }
  }

  uint8_t c3;
  if (ads127_rreg(hspi, ADS127_REG_CONFIG3, &c3) != HAL_OK) {
    dg->fault_mask |= 1u << 6;
    return -7;
  }
  /* Wideband OSR256: FILTER=00011b, DELAY=000 → 0x03 */
  ads127_start_set(0);
  if (ads127_wreg(hspi, ADS127_REG_CONFIG3, 0x03u) != HAL_OK) {
    dg->fault_mask |= 1u << 7;
    err = -8;
  } else {
    uint8_t c3_rb = 0;
    if (ads127_rreg(hspi, ADS127_REG_CONFIG3, &c3_rb) == HAL_OK && ((c3_rb & 0x1Fu) != 0x03u)) {
      dg->fault_mask |= 1u << 12;
      err = -8;
    }
  }

  (void)ads127_shadow_refresh(hspi, sh);
  /* POR STATUS is typically not 00h; all-zero shadow strongly suggests floating MISO. */
  if (sh->dev_id == 0u && sh->rev_id == 0u && sh->status == 0u && sh->config4 == 0u) {
    dg->fault_mask |= 1u << 13;
    if (err == 0) {
      err = -10;
    }
  }
  return err;
}

HAL_StatusTypeDef ads127_read_sample24_blocking(
    SPI_HandleTypeDef *hspi,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg)
{
  const uint32_t t0 = HAL_GetTick();
  /* !CS = 0 → ≥100 ns settle → wait DRDY (MISO low) → SCLK only inside TransmitReceive. No arm-high phase. */
  cs_low();
  delay_after_cs_100ns();
  dg->drdy_skipped_arm_high = 0u;
  {
    uint32_t iter = 0u;
    for (;;) {
      if (miso_line_high_raw() == 0u) {
        break;
      }
      iter++;
      if ((iter & MISO_POLL_TICK_MASK) == 0u && (HAL_GetTick() - t0) > timeout_ms) {
        cs_high();
        dg->drdy_timeouts++;
        return HAL_TIMEOUT;
      }
    }
  }

  uint8_t tx[3] = { 0, 0, 0 };
  uint8_t rx[3];
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(hspi, tx, rx, 3u, 200u);
  cs_high();
  if (st == HAL_OK) {
    out24[0] = rx[0];
    out24[1] = rx[1];
    out24[2] = rx[2];
    dg->last_sample_u32_be = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | (uint32_t)rx[2];
  }
  return st;
}
