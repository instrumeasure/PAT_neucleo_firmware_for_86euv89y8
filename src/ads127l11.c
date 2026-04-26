#include "ads127l11.h"
#include "pat_pinmap.h"
#include "pat_spi_h7_master.h"
#include <stdio.h>
#include <string.h>

#ifndef PAT_ADS127_SPI_HAL_LEGACY
#define PAT_ADS127_SPI_HAL_LEGACY 0
#endif

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

/* TI SBAS946 §8.5.1.1: CS is active-low — 4-wire: frame starts CS low, ends CS high (MCU SET = idle). */
static void ch_cs_high(const ads127_ch_ctx_t *c)
{
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  (void)c;
#else
  HAL_GPIO_WritePin(c->cs_port, c->cs_pin, GPIO_PIN_SET);
#endif
}

static void ch_cs_low(const ads127_ch_ctx_t *c)
{
  HAL_GPIO_WritePin(c->cs_port, c->cs_pin, GPIO_PIN_RESET);
}

/**
 * Deassert every J1 !CS (idle high) in three BSRR writes: SPI1+SPI3 share GPIOA so both edges coincide
 * on one store; SPI2 (GPIOB) and SPI4 (GPIOE) one store each. Faster and tighter skew than four HAL calls.
 */
static void quartet_ncs_all_deassert_bsrr(void)
{
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  /* TI §8.5.9: taking CS high exits 3-wire — never deassert. */
#else
  const uint32_t a_cs = (uint32_t)PAT_PINMAP_SPI1_NCS_PIN | (uint32_t)PAT_PINMAP_SPI3_NCS_PIN;
  PAT_PINMAP_SPI1_NCS_PORT->BSRR = a_cs;
  PAT_PINMAP_SPI2_NCS_PORT->BSRR = (uint32_t)PAT_PINMAP_SPI2_NCS_PIN;
  PAT_PINMAP_SPI4_NCS_PORT->BSRR = (uint32_t)PAT_PINMAP_SPI4_NCS_PIN;
#endif
}

/** Deassert every J1 !CS before shared START edges or a new quartet epoch (avoids any pad left selected). */
static void ads127_ncs_all_high(void)
{
  quartet_ncs_all_deassert_bsrr();
}

/** Pin index 0..15 for a single `GPIO_PIN_n` mask (MISO lines are one-hot). */
static inline uint8_t miso_pin_pos(uint16_t pin)
{
  return (uint8_t)__builtin_ctz((unsigned)pin);
}

/**
 * H7: MISO as GPIO input so `IDR` follows SDO/DRDY (AF+SPE=0 is unreliable). Pull-up when ADC tri-states.
 * Register-only (no `HAL_GPIO_Init`) — hot path for every sample / quartet arm.
 */
static void ch_miso_enter_gpio_input(const ads127_ch_ctx_t *c)
{
  GPIO_TypeDef *const p = c->miso_port;
  const uint8_t ps = miso_pin_pos(c->miso_pin);
  const uint32_t m2 = 3uL << (uint32_t)(ps * 2u);
  p->MODER = (p->MODER & ~m2) | (0uL << (uint32_t)(ps * 2u));
  const uint32_t pup = 3uL << (uint32_t)(ps * 2u);
  p->PUPDR = (p->PUPDR & ~pup) | (1uL << (uint32_t)(ps * 2u));
  const uint32_t spd = 3uL << (uint32_t)(ps * 2u);
  p->OSPEEDR = (p->OSPEEDR & ~spd) | (3uL << (uint32_t)(ps * 2u));
}

/** Restore MISO to SPI alternate function; register-only for low latency before SCLK. */
static void ch_miso_restore_af(const ads127_ch_ctx_t *c)
{
  GPIO_TypeDef *const p = c->miso_port;
  const uint8_t ps = miso_pin_pos(c->miso_pin);
  const uint32_t af = (uint32_t)c->miso_af & 0xFuL;
  const uint32_t m2 = 3uL << (uint32_t)(ps * 2u);
  p->MODER = (p->MODER & ~m2) | (2uL << (uint32_t)(ps * 2u));
  const uint32_t pup = 3uL << (uint32_t)(ps * 2u);
  p->PUPDR = (p->PUPDR & ~pup);
  const uint32_t spd = 3uL << (uint32_t)(ps * 2u);
  p->OSPEEDR = (p->OSPEEDR & ~spd) | (3uL << (uint32_t)(ps * 2u));
  p->OTYPER &= ~((uint32_t)c->miso_pin);
  if (ps < 8u) {
    const uint32_t m = 0xFuL << (uint32_t)(ps * 4u);
    p->AFR[0] = (p->AFR[0] & ~m) | (af << (uint32_t)(ps * 4u));
  } else {
    const uint8_t sh = (uint8_t)(ps - 8u);
    const uint32_t m = 0xFuL << (uint32_t)(sh * 4u);
    p->AFR[1] = (p->AFR[1] & ~m) | (af << (uint32_t)(sh * 4u));
  }
}

static uint8_t g_ads127_dwt_poll_on;

static void ads127_dwt_poll_ensure(void)
{
  if (g_ads127_dwt_poll_on != 0u) {
    return;
  }
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  g_ads127_dwt_poll_on = 1u;
}

static uint32_t ads127_ms_to_dwt_cycles(uint32_t ms)
{
  return (uint32_t)(((uint64_t)ms * (uint64_t)SystemCoreClock) / 1000ULL);
}

/**
 * H7 SPI v2: 8-bit bidirectional 3×0x00 read without `HAL_SPI_TransmitReceive` lock/state churn.
 * Caller must have left SPE enabled and 2-line 8-bit master mode configured.
 */
static HAL_StatusTypeDef spi_master_rx3_zero_tx_unlocked(SPI_HandleTypeDef *hs, uint8_t rx[3], uint32_t xfer_timeout_ms)
{
  static const uint8_t kz[3] = {0u, 0u, 0u};
#if PAT_ADS127_SPI_HAL_LEGACY
  SPI_TypeDef *const SPIx = hs->Instance;

  if ((hs->State != HAL_SPI_STATE_READY) || (hs->Init.DataSize != SPI_DATASIZE_8BIT)
      || (hs->Init.Direction != SPI_DIRECTION_2LINES) || (hs->Init.Mode != SPI_MODE_MASTER)) {
    return HAL_SPI_TransmitReceive(hs, kz, rx, 3u, xfer_timeout_ms);
  }

  const uint32_t tick0 = HAL_GetTick();
  uint16_t tx_left = 3u;
  uint16_t rx_left = 3u;
  const uint32_t fifo_len = IS_SPI_HIGHEND_INSTANCE(SPIx) ? SPI_HIGHEND_FIFO_SIZE : SPI_LOWEND_FIFO_SIZE;
  const uint16_t fifo_pkt = (uint16_t)(((uint16_t)(hs->Init.FifoThreshold >> 5U) + 1U));
  uint8_t *prx = rx;

  MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, 3uL);
  SET_BIT(SPIx->CR1, SPI_CR1_CSTART);

  while ((tx_left > 0u) || (rx_left > 0u)) {
    if (__HAL_SPI_GET_FLAG(hs, SPI_FLAG_TXP) && (tx_left > 0u) && (rx_left < (tx_left + fifo_len))) {
      *((__IO uint8_t *)&SPIx->TXDR) = 0u;
      tx_left--;
    }
    const uint32_t sr = SPIx->SR;
    if (rx_left > 0u) {
      if (__HAL_SPI_GET_FLAG(hs, SPI_FLAG_RXP)) {
        *prx++ = *((__IO uint8_t *)&SPIx->RXDR);
        rx_left--;
      } else if ((rx_left < fifo_pkt) && ((sr & SPI_SR_RXWNE_Msk) != 0UL)) {
        *prx++ = *((__IO uint8_t *)&SPIx->RXDR);
        rx_left--;
      } else if ((rx_left < 4u) && ((sr & SPI_SR_RXPLVL_Msk) != 0UL)) {
        *prx++ = *((__IO uint8_t *)&SPIx->RXDR);
        rx_left--;
      } else {
        if ((xfer_timeout_ms != HAL_MAX_DELAY) && ((HAL_GetTick() - tick0) > xfer_timeout_ms)) {
          goto spi_cleanup_fail;
        }
      }
    }
  }

  while (!__HAL_SPI_GET_FLAG(hs, SPI_FLAG_EOT)) {
    if ((xfer_timeout_ms != HAL_MAX_DELAY) && ((HAL_GetTick() - tick0) > xfer_timeout_ms)) {
      goto spi_cleanup_fail;
    }
  }

  __HAL_SPI_CLEAR_EOTFLAG(hs);
  __HAL_SPI_CLEAR_TXTFFLAG(hs);
  __HAL_SPI_DISABLE(hs);
  __HAL_SPI_DISABLE_IT(hs, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR
                            | SPI_IT_FRE | SPI_IT_MODF));
  CLEAR_BIT(SPIx->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);
  MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, 0UL);
  return HAL_OK;

spi_cleanup_fail:
  MODIFY_REG(SPIx->CR2, SPI_CR2_TSIZE, 0UL);
  __HAL_SPI_CLEAR_EOTFLAG(hs);
  __HAL_SPI_CLEAR_TXTFFLAG(hs);
  __HAL_SPI_DISABLE(hs);
  __HAL_SPI_DISABLE_IT(hs, (SPI_IT_EOT | SPI_IT_TXP | SPI_IT_RXP | SPI_IT_DXP | SPI_IT_UDR | SPI_IT_OVR
                            | SPI_IT_FRE | SPI_IT_MODF));
  CLEAR_BIT(SPIx->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);
  return HAL_TIMEOUT;
#else
  if ((hs->State != HAL_SPI_STATE_READY) || (hs->Init.DataSize != SPI_DATASIZE_8BIT)
      || (hs->Init.Direction != SPI_DIRECTION_2LINES) || (hs->Init.Mode != SPI_MODE_MASTER)) {
    return HAL_SPI_TransmitReceive(hs, kz, rx, 3u, xfer_timeout_ms);
  }

  pat_spi_master_cfg_t pcfg;
  pat_spi_h7_master_cfg_from_hspi(hs, &pcfg);
  const uint32_t cyc = (xfer_timeout_ms == HAL_MAX_DELAY) ? UINT32_MAX : ads127_ms_to_dwt_cycles(xfer_timeout_ms);
  return pat_spi_h7_master_txrx(hs->Instance, &pcfg, kz, rx, 3u, cyc);
#endif
}

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
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  HAL_GPIO_WritePin(PAT_PINMAP_SPI1_NCS_PORT, PAT_PINMAP_SPI1_NCS_PIN, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(PAT_PINMAP_SPI1_NCS_PORT, PAT_PINMAP_SPI1_NCS_PIN, GPIO_PIN_SET);
#endif

  g.Pin = PAT_PINMAP_SPI3_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI3_NCS_PORT, &g);
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  HAL_GPIO_WritePin(PAT_PINMAP_SPI3_NCS_PORT, PAT_PINMAP_SPI3_NCS_PIN, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(PAT_PINMAP_SPI3_NCS_PORT, PAT_PINMAP_SPI3_NCS_PIN, GPIO_PIN_SET);
#endif

  g.Pin = PAT_PINMAP_SPI2_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI2_NCS_PORT, &g);
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  HAL_GPIO_WritePin(PAT_PINMAP_SPI2_NCS_PORT, PAT_PINMAP_SPI2_NCS_PIN, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(PAT_PINMAP_SPI2_NCS_PORT, PAT_PINMAP_SPI2_NCS_PIN, GPIO_PIN_SET);
#endif

  g.Pin = PAT_PINMAP_SPI4_NCS_PIN;
  HAL_GPIO_Init(PAT_PINMAP_SPI4_NCS_PORT, &g);
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);
#endif

  g.Pin = PIN_RST | PIN_START;
  HAL_GPIO_Init(PORT_RST, &g);
  HAL_GPIO_WritePin(PORT_RST, PIN_RST, GPIO_PIN_SET);
  HAL_GPIO_WritePin(PORT_START, PIN_START, GPIO_PIN_RESET);
}

void ads127_cs_probe_pulse_ms(uint32_t ms_low)
{
#if PAT_ADS127_SPI_3WIRE_CS_HELD_LOW
  (void)ms_low;
  return;
#else
  /* SPI4 !CS (PE11) only — LA / continuity before SPI clocks (single-channel bring-up aid). */
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);
  HAL_Delay(2u);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_RESET);
  if (ms_low != 0u) {
    HAL_Delay(ms_low);
  }
  HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);
#endif
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
  HAL_StatusTypeDef st;
#if PAT_ADS127_SPI_HAL_LEGACY
  st = HAL_SPI_TransmitReceive(ch->hspi, (uint8_t *)tx, rx, len, 200u);
#else
  if ((ch->hspi->State != HAL_SPI_STATE_READY) || (ch->hspi->Init.DataSize != SPI_DATASIZE_8BIT)
      || (ch->hspi->Init.Direction != SPI_DIRECTION_2LINES) || (ch->hspi->Init.Mode != SPI_MODE_MASTER)) {
    st = HAL_SPI_TransmitReceive(ch->hspi, (uint8_t *)tx, rx, len, 200u);
  } else {
    pat_spi_master_cfg_t pcfg;
    pat_spi_h7_master_cfg_from_hspi(ch->hspi, &pcfg);
    st = pat_spi_h7_master_txrx(ch->hspi->Instance, &pcfg, tx, rx, len, ads127_ms_to_dwt_cycles(200u));
  }
#endif
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

int ads127_bringup_no_nreset(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg)
{
  int err = 0;
  dg->fault_mask = 0u;

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
  if (ads127_wreg(hspi, ADS127_REG_CONFIG3, ADS127_CONFIG3_FILTER_WIDEBAND_OSR512) != HAL_OK) {
    dg->fault_mask |= 1u << 7;
    err = -8;
  } else {
    uint8_t c3_rb = 0;
    if (ads127_rreg(hspi, ADS127_REG_CONFIG3, &c3_rb) == HAL_OK
        && ((c3_rb & 0x1Fu) != ADS127_CONFIG3_FILTER_WIDEBAND_OSR512)) {
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

int ads127_bringup(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg)
{
  ads127_nreset_pulse();
  return ads127_bringup_no_nreset(hspi, sh, dg);
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
    printf("[12]CFG3_FILTER_neq_OS512 ");
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
  ads127_ncs_all_high();
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
    if (!all_zero && (sh->config4 & 0x80u) != 0u && (sh->config3 & 0x1Fu) == ADS127_CONFIG3_FILTER_WIDEBAND_OSR512
        && (sh->config2 & 0x20u) != 0u) {
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
  if ((sh->config3 & 0x1Fu) != ADS127_CONFIG3_FILTER_WIDEBAND_OSR512) {
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
  ads127_ncs_all_high();
  ads127_start_set(0);
  HAL_Delay(2u);
  ads127_start_set(1);
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
}

void ads127_halt_streaming_fault(const char *msg)
{
  ads127_ncs_all_high();
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
  ch_cs_low(ch);
  delay_after_cs_100ns();

  SPI_HandleTypeDef *const hs = ch->hspi;
  __HAL_SPI_DISABLE(hs);
  __DSB();
  ch_miso_enter_gpio_input(ch);
  dg->drdy_skipped_arm_high = (ch_miso_high_raw(ch) == 0u) ? 1u : 0u;
  {
    ads127_dwt_poll_ensure();
    const uint32_t cyc_lim = ads127_ms_to_dwt_cycles(timeout_ms);
    const uint32_t c0 = DWT->CYCCNT;
    for (;;) {
      if (ch_miso_high_raw(ch) == 0u) {
        break;
      }
      if ((uint32_t)(DWT->CYCCNT - c0) > cyc_lim) {
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

  uint8_t rx[3];
  HAL_StatusTypeDef st = spi_master_rx3_zero_tx_unlocked(hs, rx, 200u);
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

static uint32_t ads127_quartet_acquired_count;

uint32_t ads127_get_quartet_acquired_count(void)
{
  return ads127_quartet_acquired_count;
}

#if PAT_QUARTET_PARALLEL_DRDY_WAIT

/** Undo quartet parallel arm: MISO AF + SPI enable for `from`..3, then !CS high (batched when `from==0`). */
static void quartet_release_armed_from(ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS], unsigned from)
{
  for (unsigned j = from; j < ADS127_QUARTET_CHANNELS; j++) {
    const ads127_ch_ctx_t *ch = &ctxs[j];
    SPI_HandleTypeDef *const hs = ch->hspi;
    ch_miso_restore_af(ch);
    __HAL_SPI_ENABLE(hs);
    __DSB();
  }
  if (from == 0u) {
    quartet_ncs_all_deassert_bsrr();
  } else {
    for (unsigned j = from; j < ADS127_QUARTET_CHANNELS; j++) {
      ch_cs_high(&ctxs[j]);
    }
  }
}

/** IT buffers: valid until all four `HAL_SPI_TransmitReceive_IT` completions. */
static uint8_t gq_spi_tx[ADS127_QUARTET_CHANNELS][3];
static uint8_t gq_spi_rx[ADS127_QUARTET_CHANNELS][3];

#if (!defined(PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER) || (PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER == 0))        \
    && (!defined(PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED) || (PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED == 0))

static HAL_StatusTypeDef quartet_poll_all_spi_ready(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint32_t timeout_ms)
{
  const uint32_t t0 = HAL_GetTick();
  for (;;) {
    unsigned all_ready = 1u;
    for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
      if (HAL_SPI_GetState(ctxs[i].hspi) != HAL_SPI_STATE_READY) {
        all_ready = 0u;
        break;
      }
    }
    if (all_ready != 0u) {
      return HAL_OK;
    }
    if ((HAL_GetTick() - t0) > timeout_ms) {
      return HAL_TIMEOUT;
    }
  }
}

static void quartet_abort_all_spi(ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS])
{
  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    (void)HAL_SPI_Abort_IT(ctxs[i].hspi);
  }
  (void)quartet_poll_all_spi_ready(ctxs, 50u);
}

#endif

/** Assert all four J1 !CS (active low): SPI1+SPI3 on one GPIOA BSRR; SPI2 PB4; SPI4 PE11. */
static void quartet_ncs_all_assert_bsrr(void)
{
  const uint32_t a_cs = (uint32_t)PAT_PINMAP_SPI1_NCS_PIN | (uint32_t)PAT_PINMAP_SPI3_NCS_PIN;
  PAT_PINMAP_SPI1_NCS_PORT->BSRR = a_cs << 16u;
  PAT_PINMAP_SPI2_NCS_PORT->BSRR = (uint32_t)PAT_PINMAP_SPI2_NCS_PIN << 16u;
  PAT_PINMAP_SPI4_NCS_PORT->BSRR = (uint32_t)PAT_PINMAP_SPI4_NCS_PIN << 16u;
}

#if PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED
/**
 * With all four !CS already asserted: SPI1→SPI4 sequential `spi_master_rx3_zero_tx_unlocked`
 * (TSIZE/CSTART/EOT via `pat_spi_h7_master_txrx` — same as single-channel; no HAL SPI IT).
 */
static HAL_StatusTypeDef quartet_sample_seq_rx3_unlocked(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint8_t gq_spi_rx[ADS127_QUARTET_CHANNELS][3],
    uint8_t out24[ADS127_QUARTET_CHANNELS][3],
    ads127_diag_t dg[ADS127_QUARTET_CHANNELS])
{
  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    __HAL_SPI_ENABLE(ctxs[i].hspi);
    __DSB();
    HAL_StatusTypeDef st = spi_master_rx3_zero_tx_unlocked(ctxs[i].hspi, gq_spi_rx[i], 200u);
    if (st != HAL_OK) {
      quartet_ncs_all_deassert_bsrr();
      for (unsigned k = 0u; k < ADS127_QUARTET_CHANNELS; k++) {
        out24[k][0] = 0xFFu;
        out24[k][1] = 0xFFu;
        out24[k][2] = 0xFFu;
      }
      memset(dg, 0, sizeof(ads127_diag_t) * ADS127_QUARTET_CHANNELS);
      return st;
    }
  }
  return HAL_OK;
}
#endif

static HAL_StatusTypeDef read_quartet_blocking_parallel(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint8_t out24[ADS127_QUARTET_CHANNELS][3],
    uint32_t timeout_ms,
    ads127_diag_t dg[ADS127_QUARTET_CHANNELS])
{
#if !PAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY
  uint8_t drdy_ready[ADS127_QUARTET_CHANNELS] = {0};
#endif
  uint8_t arm_skip_snap[ADS127_QUARTET_CHANNELS];
  quartet_ncs_all_assert_bsrr();
  delay_after_cs_100ns();
#if PAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY
  /* DRDY gate reads SPI4 MISO only — only SPI4 needs SPE off + GPIO MISO for reliable IDR poll. */
  for (unsigned i = 0u; i < 3u; i++) {
    dg[i].drdy_skipped_arm_high = 0u;
    arm_skip_snap[i] = 0u;
  }
  {
    SPI_HandleTypeDef *const hs = ctxs[3].hspi;
    __HAL_SPI_DISABLE(hs);
    __DSB();
    ch_miso_enter_gpio_input(&ctxs[3]);
    dg[3].drdy_skipped_arm_high = (ch_miso_high_raw(&ctxs[3]) == 0u) ? 1u : 0u;
    arm_skip_snap[3] = dg[3].drdy_skipped_arm_high;
  }
#else
  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    SPI_HandleTypeDef *const hs = ctxs[i].hspi;
    __HAL_SPI_DISABLE(hs);
    __DSB();
    ch_miso_enter_gpio_input(&ctxs[i]);
    dg[i].drdy_skipped_arm_high = (ch_miso_high_raw(&ctxs[i]) == 0u) ? 1u : 0u;
    arm_skip_snap[i] = dg[i].drdy_skipped_arm_high;
  }
#endif

  ads127_dwt_poll_ensure();
  const uint32_t cyc_lim = ads127_ms_to_dwt_cycles(timeout_ms);
  const uint32_t c0 = DWT->CYCCNT;
  for (;;) {
#if PAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY
    if (ch_miso_high_raw(&ctxs[3]) == 0u) {
      break;
    }
#else
    for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
      if (drdy_ready[i] == 0u && ch_miso_high_raw(&ctxs[i]) == 0u) {
        drdy_ready[i] = 1u;
      }
    }
    if (drdy_ready[0u] != 0u && drdy_ready[1u] != 0u && drdy_ready[2u] != 0u && drdy_ready[3u] != 0u) {
      break;
    }
#endif
    if ((uint32_t)(DWT->CYCCNT - c0) > cyc_lim) {
      quartet_release_armed_from(ctxs, 0u);
      for (unsigned k = 0u; k < ADS127_QUARTET_CHANNELS; k++) {
        out24[k][0] = 0xFFu;
        out24[k][1] = 0xFFu;
        out24[k][2] = 0xFFu;
      }
      memset(dg, 0, sizeof(ads127_diag_t) * ADS127_QUARTET_CHANNELS);
      for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
        dg[i].drdy_skipped_arm_high = arm_skip_snap[i];
#if PAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY
        if (i == 3u && ch_miso_high_raw(&ctxs[3]) != 0u) {
          dg[i].drdy_timeouts = 1u;
        }
#else
        if (drdy_ready[i] == 0u) {
          dg[i].drdy_timeouts = 1u;
        }
#endif
      }
      return HAL_TIMEOUT;
    }
  }

  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    const ads127_ch_ctx_t *ch = &ctxs[i];
    ch_miso_restore_af(ch);
    __HAL_SPI_ENABLE(ch->hspi);
    __DSB();
  }

  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    gq_spi_tx[i][0] = 0u;
    gq_spi_tx[i][1] = 0u;
    gq_spi_tx[i][2] = 0u;
  }

#if PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED
  /* Characterisation: four sequential H7 SPIv2 unlocked 3-byte reads (no parallel IT overlap). */
  {
    HAL_StatusTypeDef st = quartet_sample_seq_rx3_unlocked(ctxs, gq_spi_rx, out24, dg);
    if (st != HAL_OK) {
      return st;
    }
  }
#elif defined(PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER) && (PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER != 0)
  /* Interleaved H7 SPIv2 on SPI1..4 (`pat_spi_h7_master.c`): overlapping SCLK vs one-bus-at-a-time seq path. */
  {
    const uint32_t cyc_sample = ads127_ms_to_dwt_cycles(200u);
    HAL_StatusTypeDef st = pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi(ctxs[0].hspi, ctxs[1].hspi, ctxs[2].hspi,
        ctxs[3].hspi, gq_spi_rx[0], gq_spi_rx[1], gq_spi_rx[2], gq_spi_rx[3], cyc_sample);
    if (st != HAL_OK) {
      quartet_ncs_all_deassert_bsrr();
      for (unsigned k = 0u; k < ADS127_QUARTET_CHANNELS; k++) {
        out24[k][0] = 0xFFu;
        out24[k][1] = 0xFFu;
        out24[k][2] = 0xFFu;
      }
      memset(dg, 0, sizeof(ads127_diag_t) * ADS127_QUARTET_CHANNELS);
      return st;
    }
  }
#else
  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    HAL_StatusTypeDef st =
        HAL_SPI_TransmitReceive_IT(ctxs[i].hspi, gq_spi_tx[i], gq_spi_rx[i], 3u);
    if (st != HAL_OK) {
      quartet_abort_all_spi(ctxs);
      quartet_ncs_all_deassert_bsrr();
      for (unsigned k = 0u; k < ADS127_QUARTET_CHANNELS; k++) {
        out24[k][0] = 0xFFu;
        out24[k][1] = 0xFFu;
        out24[k][2] = 0xFFu;
      }
      memset(dg, 0, sizeof(ads127_diag_t) * ADS127_QUARTET_CHANNELS);
      return st;
    }
  }

  HAL_StatusTypeDef wst = quartet_poll_all_spi_ready(ctxs, 200u);
  if (wst != HAL_OK) {
    quartet_abort_all_spi(ctxs);
    quartet_ncs_all_deassert_bsrr();
    for (unsigned k = 0u; k < ADS127_QUARTET_CHANNELS; k++) {
      out24[k][0] = 0xFFu;
      out24[k][1] = 0xFFu;
      out24[k][2] = 0xFFu;
    }
    memset(dg, 0, sizeof(ads127_diag_t) * ADS127_QUARTET_CHANNELS);
    return wst;
  }
#endif

  quartet_ncs_all_deassert_bsrr();

  for (unsigned i = 0u; i < ADS127_QUARTET_CHANNELS; i++) {
    out24[i][0] = gq_spi_rx[i][0];
    out24[i][1] = gq_spi_rx[i][1];
    out24[i][2] = gq_spi_rx[i][2];
    dg[i].last_sample_u32_be = ((uint32_t)gq_spi_rx[i][0] << 16) | ((uint32_t)gq_spi_rx[i][1] << 8)
        | (uint32_t)gq_spi_rx[i][2];
  }

  ads127_quartet_acquired_count++;
  return HAL_OK;
}

#endif /* PAT_QUARTET_PARALLEL_DRDY_WAIT */

HAL_StatusTypeDef ads127_read_quartet_blocking(
    ads127_ch_ctx_t ctxs[ADS127_QUARTET_CHANNELS],
    uint8_t out24[ADS127_QUARTET_CHANNELS][3],
    uint32_t timeout_ms,
    ads127_diag_t dg[ADS127_QUARTET_CHANNELS])
{
#if PAT_QUARTET_PARALLEL_DRDY_WAIT
  ads127_ncs_all_high();
  return read_quartet_blocking_parallel(ctxs, out24, timeout_ms, dg);
#else
  (void)ctxs;
  (void)out24;
  (void)timeout_ms;
  (void)dg;
  return HAL_ERROR;
#endif
}
