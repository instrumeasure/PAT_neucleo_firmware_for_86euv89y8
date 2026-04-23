/**
 * Quartet app: SPI1→SPI2→SPI3→SPI4 + four ADS127L11 (bare-metal cooperative epoch).
 * See examples/four-channel-spi1-4-ads127/README.md and AGENTS.md quartet_order.
 */
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "ads127l11.h"
#include "pat_spi_ads127.h"
#include "pat_quartet_app.h"
#include "pat_quartet_epoch.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if PAT_QUARTET_SYNC_BURST_EPOCHS > 0u
#define PAT_QUARTET_BURST_ENABLED 1
#else
#define PAT_QUARTET_BURST_ENABLED 0
#endif

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

static void MX_SPI_All_Init(void)
{
  if (pat_spi_ads127_apply_template(&hspi1, SPI1) != HAL_OK) {
    Error_Handler();
  }
  if (pat_spi_ads127_apply_template(&hspi2, SPI2) != HAL_OK) {
    Error_Handler();
  }
  if (pat_spi_ads127_apply_template(&hspi3, SPI3) != HAL_OK) {
    Error_Handler();
  }
  if (pat_spi_ads127_apply_template(&hspi4, SPI4) != HAL_OK) {
    Error_Handler();
  }
}

static void quartet_bind(ads127_ch_ctx_t ctx[ADS127_QUARTET_CHANNELS])
{
  ads127_ch_ctx_bind(&ctx[0u], 0u, &hspi1);
  ads127_ch_ctx_bind(&ctx[1u], 1u, &hspi2);
  ads127_ch_ctx_bind(&ctx[2u], 2u, &hspi3);
  ads127_ch_ctx_bind(&ctx[3u], 3u, &hspi4);
}

static void dwt_cycle_counter_enable(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t cycles_to_us_u32(uint32_t cyc)
{
  if (SystemCoreClock == 0u) {
    return 0u;
  }
  const uint64_t us = ((uint64_t)cyc * 1000000ULL) / (uint64_t)SystemCoreClock;
  return (us > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)us;
}

int main(void)
{
  HAL_Init();
  PAT_SystemClock_Config();
  dwt_cycle_counter_enable();

  MX_GPIO_LED_Init();
  MX_USART3_UART_Init();
  setvbuf(stdout, NULL, _IONBF, 0);

  {
    static const uint8_t kBoot[] =
        "\r\nPAT quartet: USART3 alive (115200). SPI1-4 + four ADS127L11 epoch.\r\n";
    (void)HAL_UART_Transmit(&huart3, kBoot, (uint16_t)(sizeof(kBoot) - 1u), 500u);
  }

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

  pat_quartet_app_print_sync_debug_boot();

  ads127_shadow_t sh[ADS127_QUARTET_CHANNELS];
  ads127_diag_t dg_bringup[ADS127_QUARTET_CHANNELS];
  int br_ch[ADS127_QUARTET_CHANNELS];
  SPI_HandleTypeDef *hs[ADS127_QUARTET_CHANNELS] = { &hspi1, &hspi2, &hspi3, &hspi4 };

  memset(sh, 0, sizeof(sh));
  memset(dg_bringup, 0, sizeof(dg_bringup));
  memset(br_ch, 0, sizeof(br_ch));

  const unsigned all_ok = pat_quartet_app_bringup_retry_all(hs, sh, dg_bringup, br_ch);
  if (all_ok == 0u) {
#ifdef PAT_ADS127_STRICT_BRINGUP
    ads127_halt_streaming_fault("Quartet: one or more channels failed bring-up after nRESET retry.");
#else
    printf("WARNING: quartet bring-up incomplete; START and epoch will run (strict bring-up OFF).\r\n");
#endif
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
  pat_quartet_app_post_start_gates_nonstrict(hs, sh, br_ch, dg_bringup);
#endif

  ads127_ch_ctx_t ctx[ADS127_QUARTET_CHANNELS];
  quartet_bind(ctx);

  pat_quartet_epoch_line_t epoch_line;
  memset(&epoch_line, 0, sizeof(epoch_line));

  ads127_diag_t dg_epoch[ADS127_QUARTET_CHANNELS];
  memset(dg_epoch, 0, sizeof(dg_epoch));

  uint8_t samp[ADS127_QUARTET_CHANNELS][3] = {{0}};
  HAL_StatusTypeDef rs =
      ads127_read_quartet_blocking(ctx, samp, QUARTET_DRDY_TIMEOUT_MS, dg_epoch);
  if (rs != HAL_OK) {
    memset(samp, 0xFF, sizeof(samp));
  } else {
    pat_quartet_epoch_line_publish(&epoch_line, samp);
  }

  printf("first quartet st=%u (SPI1->4) quartets_ok_total=%lu\r\n",
         (unsigned)rs,
         (unsigned long)ads127_get_quartet_acquired_count());
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
  uint32_t log_epoch_seq = 0u;
  uint32_t burst_done = 0u;
  uint32_t quartet_fail_total = 0u;

  for (;;) {
    memset(dg_epoch, 0, sizeof(dg_epoch));
    const uint32_t t0_ms = HAL_GetTick();
    const uint32_t c0 = DWT->CYCCNT;
    log_epoch_seq++;

    rs = ads127_read_quartet_blocking(ctx, samp, QUARTET_DRDY_TIMEOUT_MS, dg_epoch);
    const uint32_t c1 = DWT->CYCCNT;
    const uint32_t t1_ms = HAL_GetTick();
    const uint32_t span_us = cycles_to_us_u32(c1 - c0);

    if (rs != HAL_OK) {
      memset(samp, 0xFF, sizeof(samp));
      pat_quartet_epoch_line_invalidate(&epoch_line);
      quartet_fail_total++;
    } else {
      pat_quartet_epoch_line_publish(&epoch_line, samp);
    }

    const uint32_t now = HAL_GetTick();
#if PAT_QUARTET_BURST_ENABLED
    const unsigned burst_active = (burst_done < PAT_QUARTET_SYNC_BURST_EPOCHS) ? 1u : 0u;
#else
    const unsigned burst_active = 0u;
#endif

    const int do_summary =
        burst_active || ((now - log_ms) >= PAT_QUARTET_SYNC_SUMMARY_MS);
    if (burst_active) {
      burst_done++;
    }
    if (do_summary && !burst_active) {
      log_ms = now;
    }

    if (do_summary) {
      HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
      const uint32_t qok = ads127_get_quartet_acquired_count();
      const uint32_t samples_24b_ok = qok * 4u;
      printf("CNT,tick_ms=%lu,epoch_seq=%lu,quartets_ok_total=%lu,samples_24b_ok_total=%lu,quartet_fail_total=%lu\r\n",
             (unsigned long)now,
             (unsigned long)log_epoch_seq,
             (unsigned long)qok,
             (unsigned long)samples_24b_ok,
             (unsigned long)quartet_fail_total);

      const unsigned ok_mask = (rs == HAL_OK) ? 0x0Fu : 0u;
      const HAL_StatusTypeDef st_all = rs;

      printf("EPOCH,epoch_seq=%lu,t_start_ms=%lu,t_end_ms=%lu,span_us=%lu,st_all=%u,ok_mask=%u,quartets_ok_total=%lu\r\n",
             (unsigned long)log_epoch_seq,
             (unsigned long)t0_ms,
             (unsigned long)t1_ms,
             (unsigned long)span_us,
             (unsigned)st_all,
             ok_mask,
             (unsigned long)qok);

      for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
        uint32_t u24 =
            ((uint32_t)samp[c][0] << 16) | ((uint32_t)samp[c][1] << 8) | (uint32_t)samp[c][2];
        int32_t s24 = pat_quartet_sign_extend_u24(u24);
        printf("CH,epoch_seq=%lu,ch=%u,tick_ms=%lu,raw24_hex=%06lX,sdec=%ld,st=%u,to=%lu,arm_skip=%u\r\n",
               (unsigned long)log_epoch_seq,
               (unsigned)c,
               (unsigned long)now,
               (unsigned long)(u24 & 0xFFFFFFu),
               (long)s24,
               (unsigned)(uint8_t)rs,
               (unsigned long)dg_epoch[c].drdy_timeouts,
               (unsigned)dg_epoch[c].drdy_skipped_arm_high);
      }

    }
  }
}
