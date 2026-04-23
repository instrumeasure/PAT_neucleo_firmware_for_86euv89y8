/** Default app: single SPI4 + ADS127L11 (logical ch3). Reference: examples/single-channel-spi4-ads127/README.md */
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "pat_spi_ads127.h"
#include "ads127l11.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

UART_HandleTypeDef huart3;
SPI_HandleTypeDef hspi4;

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

static void MX_SPI4_Init(void)
{
  if (pat_spi_ads127_apply_template(&hspi4, SPI4) != HAL_OK) {
    Error_Handler();
  }
}

int main(void)
{
  HAL_Init();
  /* PLL + SysTick reload (stm32h7-hal-pitfalls: tick after clock change). */
  PAT_SystemClock_Config();

  MX_GPIO_LED_Init();
  MX_USART3_UART_Init();
  setvbuf(stdout, NULL, _IONBF, 0);

  /* Raw TX so a terminal opened any time after reset still sees life (no printf dependency). */
  {
    static const uint8_t kBoot[] = "\r\nPAT: USART3 alive (115200 8N1 PD8/PD9). Reset Nucleo to see full log.\r\n";
    (void)HAL_UART_Transmit(&huart3, kBoot, (uint16_t)(sizeof(kBoot) - 1u), 500u);
  }

  MX_SPI4_Init();
  ads127_pins_init();
  /* PE11=!CS: SPI frames are short on LA; slow pulse proves GPIO before bringup. */
  printf("LA: 12 ms active-low !CS pulse on PE11 (then bringup SPI).\r\n");
  ads127_cs_probe_pulse_ms(12u);

  uint32_t spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
  /* Must match MX_SPI4_Init() BaudRatePrescaler (_16 → divide kernel clock by 16). */
  const uint32_t spi4_presc = 16u;
  uint32_t f_sclk_hz = spi_ker_hz / spi4_presc;

  printf("\r\nPAT Milestone 1 - SPI4 ADS127L11 logical ch3 + USART3\r\n");
  printf("SYSCLK_Hz=%lu SPI4_kernel_Hz=%lu f_SCLK_hz~%lu (presc/%lu)\r\n",
         (unsigned long)SystemCoreClock,
         (unsigned long)spi_ker_hz,
         (unsigned long)f_sclk_hz,
         (unsigned long)spi4_presc);

  ads127_shadow_t sh;
  ads127_diag_t dg;
  memset(&sh, 0, sizeof(sh));
  memset(&dg, 0, sizeof(dg));

  int br = ads127_bringup_retry(&hspi4, &sh, &dg, 2u);
  printf("ads127_bringup(last after up to 2 tries)=%d fault_mask=0x%08lX\r\n", br, (unsigned long)dg.fault_mask);
  ads127_print_fault_mask(dg.fault_mask);
  const int bu_ok = ads127_bringup_ok(br, dg.fault_mask);
  if (!bu_ok) {
    printf("ADS127: verify J1 ch3 (PE11 !CS, PE12 SCK, PE6 MOSI, PE13 MISO, PF0 nRESET, PF1 START), HAT power, 25 MHz modulator CLK.\r\n");
    printf("shadow 00-08: %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
           sh.dev_id, sh.rev_id, sh.status, sh.control, sh.mux,
           sh.config1, sh.config2, sh.config3, sh.config4);
#ifdef PAT_ADS127_STRICT_BRINGUP
    ads127_halt_streaming_fault("SPI4 bring-up failed (fault_mask or exit code) after nRESET retry.");
#else
    printf("WARNING: bring-up incomplete; streaming anyway (reconfigure cmake -DPAT_ADS127_STRICT_BRINGUP=ON to halt).\r\n");
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
  if (ads127_rreg(&hspi4, ADS127_REG_STATUS, &st2) == HAL_OK) {
    printf("STATUS(method2)=%02X DRDY_bit=%u\r\n", st2, (unsigned)(st2 & 1u));
  }

  ads127_start_set(1);
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);
  if (bu_ok) {
    int pg = ads127_post_start_gate(&hspi4, &sh);
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
        why = "post-START CONFIG3 filter field not OS256 (0x03)";
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
  HAL_StatusTypeDef rs = ads127_read_sample24_blocking(&hspi4, samp, 10u, &dg);
  printf("sample24 st=%u drdy_timeouts=%lu drdy_arm_skip=%u B=%02X%02X%02X\r\n",
         (unsigned)rs, (unsigned long)dg.drdy_timeouts, (unsigned)dg.drdy_skipped_arm_high,
         samp[0], samp[1], samp[2]);

  /* Continuous conversions: each iteration blocks on DRDY then clocks 24b — rate follows ADC ODR.
   * Log and LED toggle ~1 Hz only so UART does not starve the read loop. */
  uint32_t log_ms = HAL_GetTick();
  for (;;) {
    rs = ads127_read_sample24_blocking(&hspi4, samp, 10u, &dg);
    uint32_t now = HAL_GetTick();
    if ((now - log_ms) >= 1000u) {
      log_ms = now;
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      uint32_t u24 =
          ((uint32_t)samp[0] << 16) | ((uint32_t)samp[1] << 8) | (uint32_t)samp[2];
      int32_t s24 = (int32_t)((u24 & 0xFFFFFFu) << 8) >> 8;
      printf("ADC,ch3,tick_ms=%lu,raw24=0x%06lX,sdec=%ld,st=%u,to=%lu,arm_skip=%u\r\n",
             (unsigned long)now,
             (unsigned long)(u24 & 0xFFFFFFu),
             (long)s24,
             (unsigned)rs,
             (unsigned long)dg.drdy_timeouts,
             (unsigned)dg.drdy_skipped_arm_high);
    }
  }
}
