/**
 * SPI6 slave bring-up (J2): 64-byte IT; TX payload regenerated after each master transfer.
 * Build: cmake --build <build-dir> --target pat_nucleo_spi6
 * Flash:  Flash-Stm32CubeOpenOCD.ps1 -Elf cmake-build/pat_nucleo_spi6.elf
 */
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "spi6_test_frame.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart3;
SPI_HandleTypeDef hspi6;

static uint8_t s_tx[SPI6_TEST_FRAME_N];
static uint8_t s_rx[SPI6_TEST_FRAME_N];

volatile uint32_t g_spi6_txrx_complete_count;
volatile uint32_t g_spi6_error_count;

/** Snapshot for USART heartbeat (written from completion callback only). */
volatile uint32_t g_hb_last_completed_idx;
volatile uint8_t g_hb_tx0, g_hb_tx1, g_hb_tx2, g_hb_tx3, g_hb_tx4;
volatile uint8_t g_hb_rx0, g_hb_rx1, g_hb_rx2;

void Error_Handler(void);

int _write(int fd, const char *ptr, int len)
{
  (void)fd;
  if (HAL_UART_Transmit(&huart3, (uint8_t *)ptr, (uint16_t)len, 1000U) != HAL_OK) {
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
  huart3.Init.BaudRate = 115200U;
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

static void MX_SPI6_Slave_Init(void)
{
  hspi6.Instance = SPI6;
  hspi6.Init.Mode = SPI_MODE_SLAVE;
  hspi6.Init.Direction = SPI_DIRECTION_2LINES;
  hspi6.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi6.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi6.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi6.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi6.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi6.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi6.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi6.Init.CRCPolynomial = 0x7U;
  hspi6.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi6.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi6.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi6.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi6.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi6.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi6.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi6.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi6.Init.IOSwap = SPI_IO_SWAP_DISABLE;

  if (HAL_SPI_Init(&hspi6) != HAL_OK) {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(SPI6_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(SPI6_IRQn);
}

void SPI6_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi6);
}

static void spi6_snapshot_for_hb(uint32_t completed_idx)
{
  g_hb_last_completed_idx = completed_idx;
  g_hb_tx0 = s_tx[0];
  g_hb_tx1 = s_tx[1];
  g_hb_tx2 = s_tx[2];
  g_hb_tx3 = s_tx[3];
  g_hb_tx4 = s_tx[4];
  g_hb_rx0 = s_rx[0];
  g_hb_rx1 = s_rx[1];
  g_hb_rx2 = s_rx[2];
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != &hspi6) {
    return;
  }
  uint32_t done_idx = g_spi6_txrx_complete_count;
  g_spi6_txrx_complete_count++;
  spi6_test_frame_fill(s_tx, s_rx, done_idx);
  spi6_snapshot_for_hb(done_idx);
  (void)HAL_SPI_TransmitReceive_IT(&hspi6, s_tx, s_rx, SPI6_TEST_FRAME_N);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != &hspi6) {
    return;
  }
  g_spi6_error_count++;
  spi6_test_frame_fill(s_tx, NULL, SPI6_TEST_FRAME_INDEX_IDLE);
  spi6_snapshot_for_hb(SPI6_TEST_FRAME_INDEX_IDLE);
  (void)HAL_SPI_TransmitReceive_IT(&hspi6, s_tx, s_rx, SPI6_TEST_FRAME_N);
}

void Error_Handler(void)
{
  while (1) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(200);
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
    static const char kBoot[] =
        "\r\nPAT: SPI6 slave (J2) test payload after each 64B master xfer. USART3 115200.\r\n";
    (void)HAL_UART_Transmit(&huart3, (uint8_t *)kBoot, (uint16_t)(sizeof(kBoot) - 1U), 500U);
  }

  memset(s_rx, 0U, sizeof(s_rx));
  spi6_test_frame_fill(s_tx, NULL, SPI6_TEST_FRAME_INDEX_IDLE);
  spi6_snapshot_for_hb(SPI6_TEST_FRAME_INDEX_IDLE);

  MX_SPI6_Slave_Init();

  if (HAL_SPI_TransmitReceive_IT(&hspi6, s_tx, s_rx, SPI6_TEST_FRAME_N) != HAL_OK) {
    printf("HAL_SPI_TransmitReceive_IT start failed\r\n");
    Error_Handler();
  }

  for (;;) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    printf("HB frames=%lu err=%lu done_idx=%lu tx[0..4]=%02X%02X%02X%02X%02X rx[0..2]=%02X%02X%02X\r\n",
           (unsigned long)g_spi6_txrx_complete_count,
           (unsigned long)g_spi6_error_count,
           (unsigned long)g_hb_last_completed_idx,
           (unsigned)g_hb_tx0,
           (unsigned)g_hb_tx1,
           (unsigned)g_hb_tx2,
           (unsigned)g_hb_tx3,
           (unsigned)g_hb_tx4,
           (unsigned)g_hb_rx0,
           (unsigned)g_hb_rx1,
           (unsigned)g_hb_rx2);
    HAL_Delay(1000);
  }
}
