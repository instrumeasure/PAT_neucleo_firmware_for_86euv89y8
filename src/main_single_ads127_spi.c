/**
 * Single ADS127L11 on one SPI bus (build with PAT_ADS127_SINGLE_SPI_BUS=1..4).
 * Same flow as `main.c` (SPI4 default build): USART3, bring-up retry, optional strict gate, 1 Hz samples.
 * See `PINMAP.md` / `include/pat_pinmap.h`.
 */
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "pat_spi_ads127.h"
#include "ads127l11.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if !defined(PAT_ADS127_SINGLE_SPI_BUS) || (PAT_ADS127_SINGLE_SPI_BUS < 1) || (PAT_ADS127_SINGLE_SPI_BUS > 4)
#error "Build with -DPAT_ADS127_SINGLE_SPI_BUS=1|2|3|4 (CMake sets this per target)."
#endif

UART_HandleTypeDef huart3;
SPI_HandleTypeDef hspi;

#if PAT_ADS127_SINGLE_SPI_BUS == 1
#define PAT_SPI_INSTANCE SPI1
#define PAT_LOG_CH       0u
#define PAT_SPI_LABEL    "SPI1"
#elif PAT_ADS127_SINGLE_SPI_BUS == 2
#define PAT_SPI_INSTANCE SPI2
#define PAT_LOG_CH       1u
#define PAT_SPI_LABEL    "SPI2"
#elif PAT_ADS127_SINGLE_SPI_BUS == 3
#define PAT_SPI_INSTANCE SPI3
#define PAT_LOG_CH       2u
#define PAT_SPI_LABEL    "SPI3"
#else
#define PAT_SPI_INSTANCE SPI4
#define PAT_LOG_CH       3u
#define PAT_SPI_LABEL    "SPI4"
#endif

static const char *pat_j1_hint(void)
{
#if PAT_ADS127_SINGLE_SPI_BUS == 1
  return "SPI1 !CS=PA4 SCK=PG11 MOSI=PD7 MISO=PG9";
#elif PAT_ADS127_SINGLE_SPI_BUS == 2
  return "SPI2 !CS=PB4 SCK=PB10 MOSI=PB15 MISO=PC2 (PC2SO in ads127_pins_init)";
#elif PAT_ADS127_SINGLE_SPI_BUS == 3
  return "SPI3 !CS=PA15 SCK=PC10 MOSI=PD6 MISO=PC11";
#else
  return "SPI4 !CS=PE11 SCK=PE12 MOSI=PE6 MISO=PE13";
#endif
}

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

static void MX_SPI_Init(void)
{
  if (pat_spi_ads127_apply_template(&hspi, PAT_SPI_INSTANCE) != HAL_OK) {
    Error_Handler();
  }
}

int main(void)
{
  HAL_Init();
  PAT_SystemClock_Config();

  MX_GPIO_LED_Init();
  MX_USART3_UART_Init();
  setvbuf(stdout, NULL, _IONBF, 0);

  {
    char kBoot[120];
    int n = snprintf(kBoot, sizeof(kBoot),
                     "\r\nPAT single ADS127: %s logical ch%u (pat_nucleo_spi%d_ads127). USART3 115200.\r\n",
                     PAT_SPI_LABEL,
                     (unsigned)PAT_LOG_CH,
                     PAT_ADS127_SINGLE_SPI_BUS);
    if (n > 0 && n < (int)sizeof(kBoot)) {
      (void)HAL_UART_Transmit(&huart3, (uint8_t *)kBoot, (uint16_t)n, 500u);
    }
  }

  /* SPI2 MISO (PC2 / PC2_C): SYSCFG PMCR.PC2SO before HAL_SPI_MspInit muxes PC2 to AF5 — same order as quartet / spi1_4_scan. */
  ads127_pins_init();
  MX_SPI_Init();

#if PAT_ADS127_SINGLE_SPI_BUS == 4
  printf("LA: 12 ms active-low !CS pulse on PE11 (SPI4), then bring-up.\r\n");
  ads127_cs_probe_pulse_ms(12u);
#else
  printf("LA: SPI4-only !CS probe skipped; use %s per PINMAP.\r\n", PAT_SPI_LABEL);
#endif

  uint32_t spi_ker_hz;
#if PAT_ADS127_SINGLE_SPI_BUS == 4
  spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
#else
  spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
#endif
  const uint32_t presc = (PAT_ADS127_SINGLE_SPI_BUS == 4) ? 16u : 64u;
  uint32_t f_sclk_hz = spi_ker_hz / presc;

  printf("\r\nPAT single-channel %s + ADS127L11 logical ch%u + USART3\r\n", PAT_SPI_LABEL, (unsigned)PAT_LOG_CH);
  printf("%s\r\n", pat_j1_hint());
  printf("SYSCLK_Hz=%lu SPI_kernel_Hz=%lu f_SCLK_hz~%lu (presc/%lu)\r\n",
         (unsigned long)SystemCoreClock,
         (unsigned long)spi_ker_hz,
         (unsigned long)f_sclk_hz,
         (unsigned long)presc);

  ads127_shadow_t sh;
  ads127_diag_t dg;
  memset(&sh, 0, sizeof(sh));
  memset(&dg, 0, sizeof(dg));

  int br = ads127_bringup_retry(&hspi, &sh, &dg, 2u);
  printf("ads127_bringup(last after up to 2 tries)=%d fault_mask=0x%08lX\r\n", br, (unsigned long)dg.fault_mask);
  ads127_print_fault_mask(dg.fault_mask);
  const int bu_ok = ads127_bringup_ok(br, dg.fault_mask);
  if (!bu_ok) {
    printf("ADS127: verify J1 wiring for %s; PF0 nRESET, PF1 START, HAT CLK.\r\n", PAT_SPI_LABEL);
    printf("shadow 00-08: %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
           sh.dev_id, sh.rev_id, sh.status, sh.control, sh.mux,
           sh.config1, sh.config2, sh.config3, sh.config4);
#ifdef PAT_ADS127_STRICT_BRINGUP
    ads127_halt_streaming_fault("Bring-up failed (strict PAT_ADS127_STRICT_BRINGUP).");
#else
    printf("WARNING: bring-up incomplete; streaming anyway (cmake -DPAT_ADS127_STRICT_BRINGUP=ON to halt).\r\n");
#endif
  }
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

  ads127_start_set(1);
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
  if (bu_ok) {
    int pg = ads127_post_start_gate(&hspi, &sh);
    if (pg != 0) {
      printf("ads127_post_start_gate=%d shadow 00-08: %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
             pg,
             sh.dev_id, sh.rev_id, sh.status, sh.control, sh.mux,
             sh.config1, sh.config2, sh.config3, sh.config4);
#ifdef PAT_ADS127_STRICT_BRINGUP
      const char *why = "post-START shadow verify failed";
      if (pg == -1) {
        why = "post-START ads127_shadow_refresh SPI error";
      } else if (pg == -2) {
        why = "post-START CONFIG4 bit7 (external CLK) not set";
      } else if (pg == -3) {
        why = "post-START CONFIG3 filter field not wideband OSR512 (0x04)";
      } else if (pg == -4) {
        why = "post-START CONFIG2 SDO_MODE bit not set";
      } else if (pg == -5) {
        why = "post-START shadow all-zero (suspect MISO float)";
      }
      ads127_halt_streaming_fault(why);
#else
      printf("WARNING: post-START gate failed; streaming anyway (strict bring-up OFF).\r\n");
      ads127_after_failed_post_start_gate();
#endif
    }
  }

  uint8_t samp[3] = {0};
  HAL_StatusTypeDef rs = ads127_read_sample24_blocking(&hspi, samp, 10u, &dg);
  printf("sample24 st=%u drdy_timeouts=%lu drdy_arm_skip=%u B=%02X%02X%02X\r\n",
         (unsigned)rs, (unsigned long)dg.drdy_timeouts, (unsigned)dg.drdy_skipped_arm_high,
         samp[0], samp[1], samp[2]);

  uint32_t log_ms = HAL_GetTick();
  for (;;) {
    rs = ads127_read_sample24_blocking(&hspi, samp, 10u, &dg);
    uint32_t now = HAL_GetTick();
    if ((now - log_ms) >= 1000u) {
      log_ms = now;
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      uint32_t u24 =
          ((uint32_t)samp[0] << 16) | ((uint32_t)samp[1] << 8) | (uint32_t)samp[2];
      int32_t s24 = (int32_t)((u24 & 0xFFFFFFu) << 8) >> 8;
      printf("ADC,ch%u,tick_ms=%lu,raw24=0x%06lX,sdec=%ld,st=%u,to=%lu,arm_skip=%u\r\n",
             (unsigned)PAT_LOG_CH,
             (unsigned long)now,
             (unsigned long)(u24 & 0xFFFFFFu),
             (long)s24,
             (unsigned)rs,
             (unsigned long)dg.drdy_timeouts,
             (unsigned)dg.drdy_skipped_arm_high);
    }
  }
}
