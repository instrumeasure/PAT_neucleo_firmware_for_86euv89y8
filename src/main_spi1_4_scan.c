/**
 * Sequential single-bus ADS127 workflow (same as default `main.c` on SPI4): SPI1, SPI2,
 * SPI3, SPI4 — one active HAL SPI at a time, bring-up retry, post-START gate, then stream.
 * Per phase: wall clock from `MX_SPI_ApplyTemplate` through streaming is capped by
 * `SPI_BUS_SCAN_PHASE_MS` (bring-up + gate consume the same budget).
 * Use to verify J1 nets per channel. Pins: `PINMAP.md` / `include/pat_pinmap.h`.
 */
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "ads127l11.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

UART_HandleTypeDef huart3;
SPI_HandleTypeDef hspi;

/** Max wall time per SPI phase from `MX_SPI_ApplyTemplate` until handoff (bring-up + gate + stream). */
#define SPI_BUS_SCAN_PHASE_MS (3000u)
#define SPI_BUS_SCAN_DRDY_TIMEOUT_MS (10u)

void Error_Handler(void)
{
  while (1) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(200);
  }
}

int _write(int fd, const char *ptr, int len)
{
  (void)fd;
  if (HAL_UART_Transmit(&huart3, (uint8_t *)ptr, (uint16_t)len, 1000u) != HAL_OK) {
    return 0;
  }
  return len;
}

static void MX_GPIO_LED_Init(void)
{
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Pin = GPIO_PIN_0;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);
}

static void MX_USART3_UART_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200u;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  if (HAL_UART_Init(&huart3) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * SPI1–3: SPI123 kernel often 400 MHz; /64 ~6.25 MHz SCLK (was /32 ~12.5 MHz). SPI4 kernel 100 MHz; /16 ~6.25 MHz (was /8).
 */
static uint32_t mx_spi_prescaler_for_instance(const SPI_TypeDef *instance)
{
  if (instance == SPI4) {
    return SPI_BAUDRATEPRESCALER_16;
  }
  return SPI_BAUDRATEPRESCALER_64;
}

static void MX_SPI_ApplyTemplate(SPI_HandleTypeDef *hs, SPI_TypeDef *instance)
{
  memset(hs, 0, sizeof(*hs));
  hs->Instance = instance;
  hs->Init.Mode = SPI_MODE_MASTER;
  hs->Init.Direction = SPI_DIRECTION_2LINES;
  hs->Init.DataSize = SPI_DATASIZE_8BIT;
  hs->Init.CLKPolarity = SPI_POLARITY_LOW;
  hs->Init.CLKPhase = SPI_PHASE_2EDGE;
  hs->Init.NSS = SPI_NSS_SOFT;
  hs->Init.BaudRatePrescaler = mx_spi_prescaler_for_instance(instance);
  hs->Init.FirstBit = SPI_FIRSTBIT_MSB;
  hs->Init.TIMode = SPI_TIMODE_DISABLE;
  hs->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hs->Init.CRCPolynomial = 0x7U;
  hs->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hs->Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hs->Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hs->Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hs->Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hs->Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hs->Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hs->Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hs->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hs->Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(hs) != HAL_OK) {
    Error_Handler();
  }
}

typedef struct {
  SPI_TypeDef *instance;
  unsigned log_ch;
  const char *spi_name;
  const char *j1_hint;
} spi_bus_phase_t;

static const spi_bus_phase_t kPhases[] = {
    { SPI1, 0u, "SPI1", "SPI1 !CS=PA4 SCK=PG11 MOSI=PD7 MISO=PG9" },
    { SPI2, 1u, "SPI2", "SPI2 !CS=PB4 SCK=PB10 MOSI=PB15 MISO=PC2 (PC2SO in ads127_pins_init)" },
    { SPI3, 2u, "SPI3", "SPI3 !CS=PA15 SCK=PC10 MOSI=PD6 MISO=PC11" },
    { SPI4, 3u, "SPI4", "SPI4 !CS=PE11 SCK=PE12 MOSI=PE6 MISO=PE13" },
};

static void run_one_phase(const spi_bus_phase_t *ph)
{
  /* START is shared across all ADS127; stop conversions before switching active SPI / !CS domain. */
  ads127_start_set(0);
  HAL_Delay(3u);

  MX_SPI_ApplyTemplate(&hspi, ph->instance);
  const uint32_t phase_t0_ms = HAL_GetTick();

  uint32_t spi_ker_hz;
  if (ph->instance == SPI4) {
    spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
  } else {
    spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
  }
  const uint32_t presc = (uint32_t)((ph->instance == SPI4) ? 16u : 64u);
  uint32_t f_sclk_hz = spi_ker_hz / presc;

  printf("\r\n======== SPI1-4 net check: logical ch%u %s wall budget %lu ms (from SPI init) ========\r\n",
         (unsigned)ph->log_ch,
         ph->spi_name,
         (unsigned long)SPI_BUS_SCAN_PHASE_MS);
  printf("%s\r\n", ph->j1_hint);
  printf("SYSCLK_Hz=%lu SPI_kernel_Hz=%lu f_SCLK_hz~%lu (presc/%u)\r\n",
         (unsigned long)SystemCoreClock,
         (unsigned long)spi_ker_hz,
         (unsigned long)f_sclk_hz,
         (unsigned)presc);

  ads127_shadow_t sh;
  ads127_diag_t dg;
  memset(&sh, 0, sizeof(sh));
  memset(&dg, 0, sizeof(dg));

  int br = ads127_bringup_retry(&hspi, &sh, &dg, 2u);
  printf("ads127_bringup(last after up to 2 tries)=%d fault_mask=0x%08lX\r\n", br, (unsigned long)dg.fault_mask);
  ads127_print_fault_mask(dg.fault_mask);
  printf("shadow 00-08: %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
         sh.dev_id, sh.rev_id, sh.status, sh.control, sh.mux,
         sh.config1, sh.config2, sh.config3, sh.config4);

  const uint32_t f_data_nom = 49000u;
  uint32_t f_min_sclk = f_data_nom * 4u;
  printf("TI t_c(SC) floor check: 4*f_DATA_nom~%lu Hz <= f_SCLK %s\r\n",
         (unsigned long)f_min_sclk, (f_sclk_hz >= f_min_sclk) ? "OK" : "LOW");

  uint8_t st2 = 0;
  if (ads127_rreg(&hspi, ADS127_REG_STATUS, &st2) == HAL_OK) {
    printf("STATUS(method2)=%02X DRDY_bit=%u\r\n", st2, (unsigned)(st2 & 1u));
  }

  const int bu_ok = ads127_bringup_ok(br, dg.fault_mask);
  if (!bu_ok) {
#ifdef PAT_ADS127_STRICT_BRINGUP
    printf("phase ch%u: bring-up failed (strict) — skipping this phase\r\n", (unsigned)ph->log_ch);
    (void)HAL_SPI_DeInit(&hspi);
    return;
#else
    printf("WARNING phase ch%u: bring-up incomplete; stream without post-START gate.\r\n",
           (unsigned)ph->log_ch);
#endif
  }

  ads127_start_set(1);
  /* Settle before RREG in post_start_gate (avoid mis-read while filter arms). */
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
  if (bu_ok) {
    int pg = ads127_post_start_gate(&hspi, &sh);
    if (pg != 0) {
#ifdef PAT_ADS127_STRICT_BRINGUP
      printf("phase ch%u: ads127_post_start_gate=%d (strict) — skipping this phase\r\n",
             (unsigned)ph->log_ch, pg);
      ads127_start_set(0);
      (void)HAL_SPI_DeInit(&hspi);
      return;
#else
      printf("WARNING phase ch%u: post_start_gate=%d; streaming anyway.\r\n",
             (unsigned)ph->log_ch, pg);
      ads127_after_failed_post_start_gate();
#endif
    }
  }

  uint8_t samp[3] = {0};
  HAL_StatusTypeDef rs = HAL_OK;
  const uint32_t elapsed_setup_ms = HAL_GetTick() - phase_t0_ms;
  if (elapsed_setup_ms >= SPI_BUS_SCAN_PHASE_MS) {
    printf("phase ch%u: %lums >= phase budget (%lums) before stream — no sample loop\r\n",
           (unsigned)ph->log_ch,
           (unsigned long)elapsed_setup_ms,
           (unsigned long)SPI_BUS_SCAN_PHASE_MS);
  } else {
    uint32_t log_ms = HAL_GetTick();
    unsigned banner_done = 0u;
    for (;;) {
      if ((HAL_GetTick() - phase_t0_ms) >= SPI_BUS_SCAN_PHASE_MS) {
        break;
      }
      rs = ads127_read_sample24_blocking(&hspi, samp, SPI_BUS_SCAN_DRDY_TIMEOUT_MS, &dg);
      if (banner_done == 0u) {
        printf("sample24 st=%u drdy_timeouts=%lu drdy_arm_skip=%u B=%02X%02X%02X\r\n",
               (unsigned)rs, (unsigned long)dg.drdy_timeouts, (unsigned)dg.drdy_skipped_arm_high,
               samp[0], samp[1], samp[2]);
        banner_done = 1u;
      }
      uint32_t now = HAL_GetTick();
      if ((now - log_ms) >= 1000u) {
        log_ms = now;
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
        uint32_t u24 =
            ((uint32_t)samp[0] << 16) | ((uint32_t)samp[1] << 8) | (uint32_t)samp[2];
        int32_t s24 = (int32_t)((u24 & 0xFFFFFFu) << 8) >> 8;
        printf("ADC,ch%u,tick_ms=%lu,raw24=0x%06lX,sdec=%ld,st=%u,to=%lu,arm_skip=%u\r\n",
               (unsigned)ph->log_ch,
               (unsigned long)now,
               (unsigned long)(u24 & 0xFFFFFFu),
               (long)s24,
               (unsigned)rs,
               (unsigned long)dg.drdy_timeouts,
               (unsigned)dg.drdy_skipped_arm_high);
      }
    }
    printf("phase ch%u: wall_elapsed_ms~%lu (budget %lu)\r\n",
           (unsigned)ph->log_ch,
           (unsigned long)(HAL_GetTick() - phase_t0_ms),
           (unsigned long)SPI_BUS_SCAN_PHASE_MS);
  }

  ads127_start_set(0);
  (void)HAL_SPI_DeInit(&hspi);
}

int main(void)
{
  HAL_Init();
  PAT_SystemClock_Config();

  MX_GPIO_LED_Init();
  MX_USART3_UART_Init();
  setvbuf(stdout, NULL, _IONBF, 0);

  {
    static const uint8_t kBoot[] =
        "\r\nPAT pat_nucleo_spi1_4_scan: SPI1-3 SCLK presc/64 (~6.25MHz), SPI4 presc/16 — if log shows wrong presc, reflash this ELF.\r\n";
    (void)HAL_UART_Transmit(&huart3, kBoot, (uint16_t)(sizeof(kBoot) - 1u), 500u);
  }

  ads127_pins_init();

  printf("\r\nPAT SPI1-4 net check (%lu ms wall budget/phase from SPI init); presc/64 SPI1-3, presc/16 SPI4.\r\n",
         (unsigned long)SPI_BUS_SCAN_PHASE_MS);

  for (;;) {
    for (unsigned i = 0u; i < (unsigned)(sizeof(kPhases) / sizeof(kPhases[0])); i++) {
      run_one_phase(&kPhases[i]);
    }
    printf("\r\n--- cycle complete; restarting with SPI1 ---\r\n");
  }
}
