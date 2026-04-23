#include "ads127l11.h"
#include "pat_pinmap.h"
#include <stdio.h>

#define PORT_RST   PAT_PINMAP_ADS127_NRESET_PORT
#define PIN_RST    PAT_PINMAP_ADS127_NRESET_PIN
#define PORT_START PAT_PINMAP_ADS127_START_PORT
#define PIN_START  PAT_PINMAP_ADS127_START_PIN

/* SBAS946 td(RSSC) ≥ 10000·t_CLK @ 25 MHz mod CLK → 400 µs; use margin for first SPI. */
#define TD_RSSC_MS 5u
/* Between RREG command frame and data frame; margin for SPI3 when conversions run (post-START shadow). */
#define RREG_INTER_FRAME_MS 5u
/* Between full RREG cycles in `ads127_shadow_refresh` — back-to-back RREGs can mis-frame on SPI3 while converting. */
#define SHADOW_RREG_GAP_MS 1u
#define POST_WREG_MS 2u

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

static inline uint32_t ch_miso_high_raw(const ads127_ch_ctx_t *c)
{
  return (c->miso_port->IDR & (uint32_t)c->miso_pin) != 0u ? 1u : 0u;
}

static void ch_cs_high(const ads127_ch_ctx_t *c)
{
  HAL_GPIO_WritePin(c->cs_port, c->cs_pin, GPIO_PIN_SET);
}

static void ch_cs_low(const ads127_ch_ctx_t *c)
{
  HAL_GPIO_WritePin(c->cs_port, c->cs_pin, GPIO_PIN_RESET);
}

/** H7: brief GPIO input on MISO so `IDR` follows SDO/DRDY (AF+SPE=0 is unreliable). Pull-up defines idle
 *  when the ADC tri-states SDO between edges (helps SPI3 PC11 / !DRDY sense). */
static void ch_miso_enter_gpio_input(const ads127_ch_ctx_t *c)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = c->miso_pin;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(c->miso_port, &g);
}

static void ch_miso_restore_af(const ads127_ch_ctx_t *c)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = c->miso_pin;
  g.Mode = GPIO_MODE_AF_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  g.Alternate = c->miso_af;
  HAL_GPIO_Init(c->miso_port, &g);
}

/* Only call HAL_GetTick every (mask+1) polls — tick dominates loop time vs IDR read. */
#define MISO_POLL_TICK_MASK 0x1FFu

void ads127_ch_ctx_bind(ads127_ch_ctx_t *ctx, unsigned ch_index, SPI_HandleTypeDef *hspi)
{
  ctx->hspi = hspi;
  switch (ch_index) {
    case 0u:
      ctx->cs_port = PAT_PINMAP_SPI1_NCS_PORT;
      ctx->cs_pin = PAT_PINMAP_SPI1_NCS_PIN;
      ctx->miso_port = PAT_PINMAP_SPI1_MISO_PORT;
      ctx->miso_pin = PAT_PINMAP_SPI1_MISO_PIN;
      ctx->miso_af = PAT_PINMAP_SPI1_MISO_AF;
      break;
    case 1u:
      ctx->cs_port = PAT_PINMAP_SPI2_NCS_PORT;
      ctx->cs_pin = PAT_PINMAP_SPI2_NCS_PIN;
      ctx->miso_port = PAT_PINMAP_SPI2_MISO_PORT;
      ctx->miso_pin = PAT_PINMAP_SPI2_MISO_PIN;
      ctx->miso_af = PAT_PINMAP_SPI2_AF;
      break;
    case 2u:
      ctx->cs_port = PAT_PINMAP_SPI3_NCS_PORT;
      ctx->cs_pin = PAT_PINMAP_SPI3_NCS_PIN;
      ctx->miso_port = PAT_PINMAP_SPI3_MISO_PORT;
      ctx->miso_pin = PAT_PINMAP_SPI3_MISO_PIN;
      ctx->miso_af = PAT_PINMAP_SPI3_SCK_MISO_AF;
      break;
    case 3u:
    default:
      ctx->cs_port = PAT_PINMAP_SPI4_NCS_PORT;
      ctx->cs_pin = PAT_PINMAP_SPI4_NCS_PIN;
      ctx->miso_port = PAT_PINMAP_SPI4_MISO_PORT;
      ctx->miso_pin = PAT_PINMAP_SPI4_MISO_PIN;
      ctx->miso_af = PAT_PINMAP_SPI4_AF;
      break;
  }
}

/** Map an initialised SPI handle to channel wiring (SPI1..SPI4 only). */
static int ctx_pack_for_hspi(ads127_ch_ctx_t *out, SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1) {
    ads127_ch_ctx_bind(out, 0u, hspi);
    return 0;
  }
  if (hspi->Instance == SPI2) {
    ads127_ch_ctx_bind(out, 1u, hspi);
    return 0;
  }
  if (hspi->Instance == SPI3) {
    ads127_ch_ctx_bind(out, 2u, hspi);
    return 0;
  }
  if (hspi->Instance == SPI4) {
    ads127_ch_ctx_bind(out, 3u, hspi);
    return 0;
  }
  return -1;
}

void ads127_pins_init(void)
{
  /* SPI2 MISO: CubeMX label PC2_C, HAL GPIOC Pin 2. STM32H7 SYSCFG_PMCR.PC2SO (ST FAQ): 0 = analog switch
   * closed, 1 = open (pads separated). For many LQFP packages the ball used for SPI2 MISO needs the switch
   * closed so the digital AF reaches the pin; forcing open (1) can float MISO / false DRDY-low / 0x000000.
   * Default: closed (CLEAR). Legacy open default: -DPAT_SPI2_PC2SO_OPEN_INSTEAD. Deprecated alias:
   * PAT_SPI2_PC2SO_CLEAR_INSTEAD (no longer needed; default is already closed). */
  __HAL_RCC_SYSCFG_CLK_ENABLE();
#if defined(PAT_SPI2_PC2SO_OPEN_INSTEAD)
  SET_BIT(SYSCFG->PMCR, SYSCFG_PMCR_PC2SO);
#else
  CLEAR_BIT(SYSCFG->PMCR, SYSCFG_PMCR_PC2SO);
#endif

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  /* Default reset AF on PA15 (JTDI) / PB4 (NJTRST) can block GPIO !CS until pads are de-inited and re-mapped. */
  HAL_GPIO_DeInit(PAT_PINMAP_SPI1_NCS_PORT, PAT_PINMAP_SPI1_NCS_PIN);
  HAL_GPIO_DeInit(PAT_PINMAP_SPI3_NCS_PORT, PAT_PINMAP_SPI3_NCS_PIN);
  HAL_GPIO_DeInit(PAT_PINMAP_SPI2_NCS_PORT, PAT_PINMAP_SPI2_NCS_PIN);
  HAL_GPIO_DeInit(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN);

  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  /* One HAL_GPIO_Init per !CS so PA15/PB4 leave debug AF cleanly (J1 quartet). */
  g.Pin = PAT_PINMAP_SPI1_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI1_NCS_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI1_NCS_PORT, PAT_PINMAP_SPI1_NCS_PIN, GPIO_PIN_SET);

  g.Pin = PAT_PINMAP_SPI3_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI3_NCS_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI3_NCS_PORT, PAT_PINMAP_SPI3_NCS_PIN, GPIO_PIN_SET);

  g.Pin = PAT_PINMAP_SPI2_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI2_NCS_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI2_NCS_PORT, PAT_PINMAP_SPI2_NCS_PIN, GPIO_PIN_SET);

  g.Pin = PAT_PINMAP_SPI4_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI4_NCS_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);

  g.Pin = PIN_RST | PIN_START;
  HAL_GPIO_Init(PORT_RST, &g);
  HAL_GPIO_WritePin(PORT_RST, PIN_RST, GPIO_PIN_SET);
  HAL_GPIO_WritePin(PORT_START, PIN_START, GPIO_PIN_RESET);
}

void ads127_cs_probe_pulse_ms(uint32_t ms_low)
{
  /* SPI4 !CS (PE11) only — LA / continuity before SPI clocks (single-channel bring-up aid). */
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);
  HAL_Delay(2u);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_RESET);
  if (ms_low != 0u) {
    HAL_Delay(ms_low);
  }
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);
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

static HAL_StatusTypeDef spi_x_ch(const ads127_ch_ctx_t *ch, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  ch_cs_low(ch);
  delay_short();
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(ch->hspi, (uint8_t *)tx, rx, len, 200u);
  delay_short();
  ch_cs_high(ch);
  return st;
}

HAL_StatusTypeDef ads127_rreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t *out)
{
  ads127_ch_ctx_t ch;
  if (ctx_pack_for_hspi(&ch, hspi) != 0) {
    return HAL_ERROR;
  }
  uint8_t tx1[2] = { ADS127_CMD_RREG(addr), 0x00u };
  uint8_t rx1[2];
  HAL_StatusTypeDef st = spi_x_ch(&ch, tx1, rx1, 2u);
  if (st != HAL_OK) {
    return st;
  }
  (void)rx1;
  HAL_Delay(RREG_INTER_FRAME_MS);

  uint8_t tx2[2] = { 0x00u, 0x00u };
  uint8_t rx2[2];
  st = spi_x_ch(&ch, tx2, rx2, 2u);
  if (st != HAL_OK) {
    return st;
  }
  *out = rx2[0];
  return HAL_OK;
}

HAL_StatusTypeDef ads127_wreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t data)
{
  ads127_ch_ctx_t ch;
  if (ctx_pack_for_hspi(&ch, hspi) != 0) {
    return HAL_ERROR;
  }
  uint8_t tx[2] = { ADS127_CMD_WREG(addr), data };
  uint8_t rx[2];
  HAL_StatusTypeDef st = spi_x_ch(&ch, tx, rx, 2u);
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
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_REV_ID, &sh->rev_id);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_STATUS, &sh->status);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_CONTROL, &sh->control);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_MUX, &sh->mux);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_CONFIG1, &sh->config1);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_CONFIG2, &sh->config2);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
  st = ads127_rreg(hspi, ADS127_REG_CONFIG3, &sh->config3);
  if (st != HAL_OK) {
    return st;
  }
  HAL_Delay(SHADOW_RREG_GAP_MS);
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
  if (sh->dev_id == 0u && sh->rev_id == 0u && sh->status == 0u && sh->config4 == 0u) {
    dg->fault_mask |= 1u << 13;
    if (err == 0) {
      err = -10;
    }
  }
  return err;
}

int ads127_bringup_ok(int bringup_err, uint32_t fault_mask)
{
  return (bringup_err == 0 && fault_mask == 0u) ? 1 : 0;
}

void ads127_print_fault_mask(uint32_t m)
{
  if (m == 0u) {
    return;
  }
  printf("fault_mask bits: ");
  if ((m & (1u << 0)) != 0u) {
    printf("[0]RREG_DEV_ID_fail ");
  }
  if ((m & (1u << 1)) != 0u) {
    printf("[1]DEV_ID_neq_00h ");
  }
  if ((m & (1u << 2)) != 0u) {
    printf("[2]RREG_CFG4_pre_fail ");
  }
  if ((m & (1u << 3)) != 0u) {
    printf("[3]WREG_CFG4_fail ");
  }
  if ((m & (1u << 4)) != 0u) {
    printf("[4]RREG_CFG2_pre_fail ");
  }
  if ((m & (1u << 5)) != 0u) {
    printf("[5]WREG_CFG2_fail ");
  }
  if ((m & (1u << 6)) != 0u) {
    printf("[6]RREG_CFG3_pre_fail ");
  }
  if ((m & (1u << 7)) != 0u) {
    printf("[7]WREG_CFG3_fail ");
  }
  if ((m & (1u << 9)) != 0u) {
    printf("[9]RREG_CFG4_post_fail ");
  }
  if ((m & (1u << 10)) != 0u) {
    printf("[10]CFG4_no_CLK_SEL_readback ");
  }
  if ((m & (1u << 11)) != 0u) {
    printf("[11]CFG2_no_SDO_MODE_readback ");
  }
  if ((m & (1u << 12)) != 0u) {
    printf("[12]CFG3_FILTER_neq_OS256 ");
  }
  if ((m & (1u << 13)) != 0u) {
    printf("[13]shadow_all_zero_suspect_float ");
  }
  printf("\r\n");
}

int ads127_bringup_retry(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg, unsigned attempts)
{
  int br = -1;
  if (attempts == 0u) {
    attempts = 1u;
  }
  for (unsigned a = 0u; a < attempts; a++) {
    if (a > 0u) {
      ads127_nreset_pulse();
      HAL_Delay(10u);
    }
    br = ads127_bringup(hspi, sh, dg);
    if (ads127_bringup_ok(br, dg->fault_mask)) {
      return br;
    }
  }
  return br;
}

int ads127_post_start_gate(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh)
{
  /* RREG during active conversion can corrupt readback (SPI3: CONFIG2/4 zero, CONFIG3 OK). Hold
   * START low so SDO is not in DRDY/data mode, refresh shadow, then restore START for streaming. */
  ads127_start_set(0);
  HAL_Delay(3u);

  for (unsigned a = 0u; a < 8u; a++) {
    if (a > 0u) {
      HAL_Delay(5u);
    }
    if (ads127_shadow_refresh(hspi, sh) != HAL_OK) {
      ads127_start_set(1);
      HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
      return -1;
    }
    const int all_zero = (sh->dev_id == 0u && sh->rev_id == 0u && sh->status == 0u && sh->config4 == 0u);
    if (!all_zero && (sh->config4 & 0x80u) != 0u && (sh->config3 & 0x1Fu) == 0x03u && (sh->config2 & 0x20u) != 0u) {
      ads127_start_set(1);
      HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
      return 0;
    }
  }
  ads127_start_set(1);
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
  if ((sh->config4 & 0x80u) == 0u) {
    return -2;
  }
  if ((sh->config3 & 0x1Fu) != 0x03u) {
    return -3;
  }
  if ((sh->config2 & 0x20u) == 0u) {
    return -4;
  }
  if (sh->dev_id == 0u && sh->rev_id == 0u && sh->status == 0u && sh->config4 == 0u) {
    return -5;
  }
  return 0;
}

void ads127_after_failed_post_start_gate(void)
{
  ads127_start_set(0);
  HAL_Delay(2u);
  ads127_start_set(1);
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
}

void ads127_halt_streaming_fault(const char *msg)
{
  ads127_start_set(0);
  printf("\r\nADS127 HALT (START off, no streaming): %s\r\n", msg);
  for (;;) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(250u);
  }
}

HAL_StatusTypeDef ads127_read_sample24_ch_blocking(
    const ads127_ch_ctx_t *ch,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg)
{
  const uint32_t t0 = HAL_GetTick();
  ch_cs_low(ch);
  delay_after_cs_100ns();

  SPI_HandleTypeDef *const hs = ch->hspi;
  __HAL_SPI_DISABLE(hs);
  __DSB();
  ch_miso_enter_gpio_input(ch);
  dg->drdy_skipped_arm_high = (ch_miso_high_raw(ch) == 0u) ? 1u : 0u;
  {
    uint32_t iter = 0u;
    for (;;) {
      if (ch_miso_high_raw(ch) == 0u) {
        break;
      }
      iter++;
      if ((iter & MISO_POLL_TICK_MASK) == 0u && (HAL_GetTick() - t0) > timeout_ms) {
        ch_miso_restore_af(ch);
        __HAL_SPI_ENABLE(hs);
        __DSB();
        ch_cs_high(ch);
        dg->drdy_timeouts++;
        return HAL_TIMEOUT;
      }
    }
  }
  ch_miso_restore_af(ch);
  __HAL_SPI_ENABLE(hs);
  __DSB();

  uint8_t tx[3] = { 0, 0, 0 };
  uint8_t rx[3];
  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(hs, tx, rx, 3u, 200u);
  ch_cs_high(ch);
  if (st == HAL_OK) {
    out24[0] = rx[0];
    out24[1] = rx[1];
    out24[2] = rx[2];
    dg->last_sample_u32_be = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | (uint32_t)rx[2];
  }
  return st;
}

HAL_StatusTypeDef ads127_read_sample24_blocking(
    SPI_HandleTypeDef *hspi,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg)
{
  ads127_ch_ctx_t ch;
  if (ctx_pack_for_hspi(&ch, hspi) != 0) {
    return HAL_ERROR;
  }
  return ads127_read_sample24_ch_blocking(&ch, out24, timeout_ms, dg);
}

HAL_StatusTypeDef ads127_read_quartet_blocking(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint8_t out24[ADS127_QUARTET_CHANNELS][3],
    uint32_t timeout_ms,
    ads127_diag_t dg[ADS127_QUARTET_CHANNELS])
{
  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    HAL_StatusTypeDef st =
        ads127_read_sample24_ch_blocking(&ctxs[i], out24[i], timeout_ms, &dg[i]);
    if (st != HAL_OK) {
      return st;
    }
  }
  return HAL_OK;
}
