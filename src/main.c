#include "stm32h7xx_hal.h"
#include "ads127l11.h"
#include <stdio.h>
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
  hspi4.Instance = SPI4;
  hspi4.Init.Mode = SPI_MODE_MASTER;
  hspi4.Init.Direction = SPI_DIRECTION_2LINES;
  hspi4.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi4.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi4.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi4.Init.NSS = SPI_NSS_SOFT;
  hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi4.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi4.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi4.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi4.Init.CRCPolynomial = 0x7U;
  hspi4.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi4.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi4.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi4.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi4.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi4.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi4.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi4.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi4.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi4.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi4) != HAL_OK) {
    Error_Handler();
  }
}

int main(void)
{
  HAL_Init();
  SystemCoreClockUpdate();
  HAL_InitTick(TICK_INT_PRIORITY);

  MX_GPIO_LED_Init();
  MX_USART3_UART_Init();
  setvbuf(stdout, NULL, _IONBF, 0);

  MX_SPI4_Init();
  ads127_pins_init();

  uint32_t spi_ker_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4);
  uint32_t f_sclk_hz = spi_ker_hz >> 5u; /* /32 prescaler */

  printf("\r\nPAT Milestone 1 — SPI4 ADS127L11 logical ch3 + USART3\r\n");
  printf("SYSCLK_Hz=%lu SPI4_kernel_Hz=%lu f_SCLK_hz~%lu (presc/32)\r\n",
         (unsigned long)SystemCoreClock, (unsigned long)spi_ker_hz, (unsigned long)f_sclk_hz);

  ads127_shadow_t sh;
  ads127_diag_t dg;
  memset(&sh, 0, sizeof(sh));
  memset(&dg, 0, sizeof(dg));

  int br = ads127_bringup(&hspi4, &sh, &dg);
  printf("ads127_bringup=%d fault_mask=0x%08lX\r\n", br, (unsigned long)dg.fault_mask);
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
  HAL_Delay(3u);

  uint8_t samp[3] = {0};
  HAL_StatusTypeDef rs = ads127_read_sample24_blocking(&hspi4, samp, 10u, &dg);
  printf("sample24 st=%u drdy_timeouts=%lu B=%02X%02X%02X\r\n",
         (unsigned)rs, (unsigned long)dg.drdy_timeouts, samp[0], samp[1], samp[2]);

  for (;;) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(500);
  }
}
