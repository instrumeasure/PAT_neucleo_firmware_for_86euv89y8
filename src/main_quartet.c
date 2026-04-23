/**
 * Quartet app: SPI1→SPI2→SPI3→SPI4 + four ADS127L11 (bare-metal cooperative epoch).
 * See examples/four-channel-spi1-4-ads127/README.md and AGENTS.md quartet_order.
 */
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "ads127l11.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

UART_HandleTypeDef huart3;
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;
SPI_HandleTypeDef hspi4;

/** DRDY wait per channel (ms). ODR ~50 ksps ⇒ ~20 µs/conv; margin for scheduling + PC2 switch bring-up. */
#define QUARTET_DRDY_TIMEOUT_MS 40u

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

/** SPI4: kernel /16 ~6.25 MHz SCLK (same idea as `main.c`). SPI1–3: SPI123 kernel often 400 MHz; /64 ~6.25 MHz (was /32). */
static uint32_t mx_spi_prescaler_for_instance(const SPI_TypeDef *instance)
{
  if (instance == SPI4) {
    return SPI_BAUDRATEPRESCALER_16;
  }
  return SPI_BAUDRATEPRESCALER_64;
}

static void MX_SPI_ApplyTemplate(SPI_HandleTypeDef *hspi, SPI_TypeDef *instance)
{
  memset(hspi, 0, sizeof(*hspi));
  hspi->Instance = instance;
  hspi->Init.Mode = SPI_MODE_MASTER;
  hspi->Init.Direction = SPI_DIRECTION_2LINES;
  hspi->Init.DataSize = SPI_DATASIZE_8BIT;
  hspi->Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi->Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi->Init.NSS = SPI_NSS_SOFT;
  hspi->Init.BaudRatePrescaler = mx_spi_prescaler_for_instance(instance);
  hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi->Init.TIMode = SPI_TIMODE_DISABLE;
  hspi->Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi->Init.CRCPolynomial = 0x7U;
  hspi->Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi->Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi->Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi->Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi->Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi->Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi->Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi->Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  /* SPI1–4: keep IO state when SPE=0 so DRDY MISO poll sees the pad (STM32H7). */
  hspi->Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi->Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(hspi) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_SPI_All_Init(void)
{
  MX_SPI_ApplyTemplate(&hspi1, SPI1);
  MX_SPI_ApplyTemplate(&hspi2, SPI2);
  MX_SPI_ApplyTemplate(&hspi3, SPI3);
  MX_SPI_ApplyTemplate(&hspi4, SPI4);
}

static void quartet_bind(ads127_ch_ctx_t ctx[ADS127_QUARTET_CHANNELS])
{
  ads127_ch_ctx_bind(&ctx[0u], 0u, &hspi1);
  ads127_ch_ctx_bind(&ctx[1u], 1u, &hspi2);
  ads127_ch_ctx_bind(&ctx[2u], 2u, &hspi3);
  ads127_ch_ctx_bind(&ctx[3u], 3u, &hspi4);
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
        "\r\nPAT quartet: USART3 alive (115200). SPI1-4 + four ADS127L11 epoch.\r\n";
    (void)HAL_UART_Transmit(&huart3, kBoot, (uint16_t)(sizeof(kBoot) - 1u), 500u);
  }

  /* All !CS outputs idle high before any SPI (see ads127_pins_init). */
  ads127_pins_init();
  MX_SPI_All_Init();

  printf("\r\nPAT Milestone quartet — SPI1->SPI2->SPI3->SPI4 scan order per epoch\r\n");
  {
    uint32_t k1 = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
    uint32_t k4 = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
    printf("SYSCLK_Hz=%lu SPI123_kernel_Hz=%lu SPI4_kernel_Hz=%lu f_SCLK_SPI1to3~%lu (presc/64) f_SCLK_SPI4~%lu (presc/16)\r\n",
           (unsigned long)SystemCoreClock,
           (unsigned long)k1,
           (unsigned long)k4,
           (unsigned long)(k1 / 64u),
           (unsigned long)(k4 / 16u));
  }

  ads127_shadow_t sh[ADS127_QUARTET_CHANNELS];
  ads127_diag_t dg_bringup[ADS127_QUARTET_CHANNELS];
  int br_ch[ADS127_QUARTET_CHANNELS];
  SPI_HandleTypeDef *hs[ADS127_QUARTET_CHANNELS] = { &hspi1, &hspi2, &hspi3, &hspi4 };

  memset(sh, 0, sizeof(sh));
  memset(dg_bringup, 0, sizeof(dg_bringup));
  memset(br_ch, 0, sizeof(br_ch));

  {
    unsigned all_ok = 0u;
    for (unsigned attempt = 0u; attempt < 2u; attempt++) {
      if (attempt > 0u) {
        printf("\r\nQuartet bring-up retry after nRESET...\r\n");
        ads127_nreset_pulse();
        HAL_Delay(15u);
      } else {
        ads127_nreset_pulse();
        HAL_Delay(5u);
      }
      all_ok = 1u;
      for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
        int br = ads127_bringup(hs[c], &sh[c], &dg_bringup[c]);
        br_ch[c] = br;
        printf("ch%u ads127_bringup=%d fault_mask=0x%08lX\r\n",
               (unsigned)c, br, (unsigned long)dg_bringup[c].fault_mask);
        ads127_print_fault_mask(dg_bringup[c].fault_mask);
        printf(
            "ch%u shadow 00-08: %02X %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
            (unsigned)c,
            sh[c].dev_id,
            sh[c].rev_id,
            sh[c].status,
            sh[c].control,
            sh[c].mux,
            sh[c].config1,
            sh[c].config2,
            sh[c].config3,
            sh[c].config4);
        if (!ads127_bringup_ok(br, dg_bringup[c].fault_mask)) {
          all_ok = 0u;
        }
      }
      if (all_ok != 0u) {
        break;
      }
    }
    if (all_ok == 0u) {
#ifdef PAT_ADS127_STRICT_BRINGUP
      ads127_halt_streaming_fault("Quartet: one or more channels failed bring-up after nRESET retry.");
#else
      printf("WARNING: quartet bring-up incomplete; START and epoch will run (strict bring-up OFF).\r\n");
#endif
    }
  }

  ads127_start_set(1);
  HAL_Delay(ADS127_START_STREAM_SETTLE_MS);

#ifdef PAT_ADS127_STRICT_BRINGUP
  for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
    int pg = ads127_post_start_gate(hs[c], &sh[c]);
    if (pg != 0) {
      printf("ch%u ads127_post_start_gate=%d\r\n", (unsigned)c, pg);
      ads127_halt_streaming_fault("Quartet: post-START register verify failed (see ch above).");
    }
  }
#else
  {
    unsigned gate_fail = 0u;
    for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
      if (!ads127_bringup_ok(br_ch[c], dg_bringup[c].fault_mask)) {
        printf("WARNING ch%u: skip post-START gate (bring-up not clean).\r\n", (unsigned)c);
        continue;
      }
      int pg = ads127_post_start_gate(hs[c], &sh[c]);
      if (pg != 0) {
        printf("WARNING ch%u post_start_gate=%d; continuing.\r\n", (unsigned)c, pg);
        gate_fail = 1u;
      }
    }
    if (gate_fail != 0u) {
      ads127_after_failed_post_start_gate();
    }
  }
#endif

  ads127_ch_ctx_t ctx[ADS127_QUARTET_CHANNELS];
  quartet_bind(ctx);

  ads127_diag_t dg_epoch[ADS127_QUARTET_CHANNELS];
  memset(dg_epoch, 0, sizeof(dg_epoch));

  uint8_t samp[ADS127_QUARTET_CHANNELS][3] = {{0}};
  HAL_StatusTypeDef rs =
      ads127_read_quartet_blocking(ctx, samp, QUARTET_DRDY_TIMEOUT_MS, dg_epoch);
  if (rs != HAL_OK) {
    memset(samp, 0xFF, sizeof(samp));
  }
  /* One line per channel: long single-line logs often truncate on VCP / terminal buffers. */
  printf("first quartet st=%u (SPI1->4)\r\n", (unsigned)rs);
  for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
    printf("  ch%u to=%lu arm_skip=%u raw=%02X%02X%02X\r\n",
           (unsigned)c,
           (unsigned long)dg_epoch[c].drdy_timeouts,
           (unsigned)dg_epoch[c].drdy_skipped_arm_high,
           samp[c][0],
           samp[c][1],
           samp[c][2]);
  }

  uint32_t log_ms = HAL_GetTick();
  for (;;) {
    memset(dg_epoch, 0, sizeof(dg_epoch));
    rs = ads127_read_quartet_blocking(ctx, samp, QUARTET_DRDY_TIMEOUT_MS, dg_epoch);
    if (rs != HAL_OK) {
      /* Do not leave stale raw bytes when an early channel fails (HAL_TIMEOUT=3). */
      memset(samp, 0xFF, sizeof(samp));
    }
    uint32_t now = HAL_GetTick();
    if ((now - log_ms) >= 1000u) {
      log_ms = now;
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      printf("epoch tick_ms=%lu st=%u\r\n", (unsigned long)now, (unsigned)rs);
      for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
        uint32_t u24 = ((uint32_t)samp[c][0] << 16) | ((uint32_t)samp[c][1] << 8) | (uint32_t)samp[c][2];
        int32_t s24 = (int32_t)((u24 & 0xFFFFFFu) << 8) >> 8;
        printf("  ch%u raw24=0x%06lX sdec=%ld to=%lu arm=%u\r\n",
               (unsigned)c,
               (unsigned long)(u24 & 0xFFFFFFu),
               (long)s24,
               (unsigned long)dg_epoch[c].drdy_timeouts,
               (unsigned)dg_epoch[c].drdy_skipped_arm_high);
      }
    }
  }
}
