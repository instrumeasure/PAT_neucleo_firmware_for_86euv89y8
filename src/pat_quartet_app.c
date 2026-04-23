#include "pat_quartet_app.h"
#include <stdio.h>
#include <string.h>

void pat_quartet_app_print_sync_debug_boot(void)
{
  printf("sync_debug summary_ms=%u burst_epochs=%u (UART cadence; acquisition runs at full loop rate)\r\n",
         (unsigned)PAT_QUARTET_SYNC_SUMMARY_MS,
         (unsigned)PAT_QUARTET_SYNC_BURST_EPOCHS);
}

static void print_ti_stat_for_channel(SPI_HandleTypeDef *h, unsigned ch)
{
  uint32_t spi_ker_hz;
  if (h->Instance == SPI4) {
    spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
  } else {
    spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
  }
  const uint32_t presc = (h->Instance == SPI4) ? 16u : 64u;
  const uint32_t f_sclk_hz = spi_ker_hz / presc;
  const uint32_t f_data_nom = 49000u;
  const uint32_t f_min_sclk = f_data_nom * 4u;
  const unsigned ok = (f_sclk_hz >= f_min_sclk) ? 1u : 0u;
  printf("TI,ch%u,f_data_nom_hz=%lu,f_sclk_hz=%lu,ok=%u\r\n",
         (unsigned)ch,
         (unsigned long)f_data_nom,
         (unsigned long)f_sclk_hz,
         ok);

  uint8_t st2 = 0;
  if (ads127_rreg(h, ADS127_REG_STATUS, &st2) == HAL_OK) {
    printf("STAT,ch%u,status=%02X,drdy_bit=%u\r\n",
           (unsigned)ch,
           st2,
           (unsigned)(st2 & 1u));
  } else {
    printf("STAT,ch%u,status=--,drdy_bit=--\r\n", (unsigned)ch);
  }
}

unsigned pat_quartet_app_bringup_retry_all(
    SPI_HandleTypeDef *hs[ADS127_QUARTET_CHANNELS],
    ads127_shadow_t sh[ADS127_QUARTET_CHANNELS],
    ads127_diag_t dg_bu[ADS127_QUARTET_CHANNELS],
    int br_ch[ADS127_QUARTET_CHANNELS])
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
      int br = ads127_bringup(hs[c], &sh[c], &dg_bu[c]);
      br_ch[c] = br;
      printf("BRU,ch%u,bringup=%d,fault_mask=0x%08lX\r\n",
             (unsigned)c,
             br,
             (unsigned long)dg_bu[c].fault_mask);
      ads127_print_fault_mask(dg_bu[c].fault_mask);
      printf(
          "SH,ch%u,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\r\n",
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
      print_ti_stat_for_channel(hs[c], c);
      if (!ads127_bringup_ok(br, dg_bu[c].fault_mask)) {
        all_ok = 0u;
      }
    }
    if (all_ok != 0u) {
      break;
    }
  }
  return all_ok;
}

void pat_quartet_app_post_start_gates_nonstrict(
    SPI_HandleTypeDef *hs[ADS127_QUARTET_CHANNELS],
    ads127_shadow_t sh[ADS127_QUARTET_CHANNELS],
    const int br_ch[ADS127_QUARTET_CHANNELS],
    const ads127_diag_t dg_bu[ADS127_QUARTET_CHANNELS])
{
  (void)dg_bu;
  unsigned gate_fail = 0u;
  for (unsigned c = 0u; c < ADS127_QUARTET_CHANNELS; c++) {
    if (!ads127_bringup_ok(br_ch[c], dg_bu[c].fault_mask)) {
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
