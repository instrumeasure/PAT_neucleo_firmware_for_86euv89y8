#include "ads127l11.h"

/* PINMAP ch3 */
#define PORT_CS   GPIOE
#define PIN_CS    GPIO_PIN_11
#define PORT_RST  GPIOF
#define PIN_RST   GPIO_PIN_0
#define PORT_START GPIOF
#define PIN_START GPIO_PIN_1
#define PORT_MISO_LINE GPIOE
#define PIN_MISO       GPIO_PIN_13

#define TD_RSSC_MS 1u /* ≥ 400 µs @ 25 MHz mod CLK; 1 ms lazy */
#define DRDY_POLL_STEP_US 5u

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
  return spi_x(hspi, tx, rx, 2u);
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
  }

  (void)ads127_shadow_refresh(hspi, sh);
  return err;
}

HAL_StatusTypeDef ads127_read_sample24_blocking(
    SPI_HandleTypeDef *hspi,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg)
{
  const uint32_t t0 = HAL_GetTick();
  /* Poll MISO / SDO–DRDY while CS low, no SCLK (method 1). */
  cs_low();
  delay_short();
  for (;;) {
    GPIO_PinState s = HAL_GPIO_ReadPin(PORT_MISO_LINE, PIN_MISO);
    /* Heuristic: treat pin low as “ready to clock” (tune vs LA / STATUS DRDY). */
    if (s == GPIO_PIN_RESET) {
      break;
    }
    if ((HAL_GetTick() - t0) > timeout_ms) {
      cs_high();
      dg->drdy_timeouts++;
      return HAL_TIMEOUT;
    }
    for (volatile uint32_t w = 0; w < 50u; w++) {
      __NOP();
    }
  }

  uint8_t tx[3] = { 0, 0, 0 };
  uint8_t rx[3];
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(hspi, tx, rx, 3u, 200u);
  delay_short();
  cs_high();
  if (st == HAL_OK) {
    out24[0] = rx[0];
    out24[1] = rx[1];
    out24[2] = rx[2];
    dg->last_sample_u32_be = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | (uint32_t)rx[2];
  }
  return st;
}
