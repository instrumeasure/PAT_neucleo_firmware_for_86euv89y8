#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "pat_pinmap.h"
#include "ad5664r.h"
#include "pat_mems_regs.h"
#include "pat_mems_sm.h"
#include "pat_uart5_pat5.h"
#include "pat_uart7_laser.h"

#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart3;
UART_HandleTypeDef huart5;
UART_HandleTypeDef huart7;
SPI_HandleTypeDef hspi5;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_uart7_rx;
DMA_HandleTypeDef hdma_uart7_tx;

static ad5664r_dev_t g_dac;
static pat_mems_reg_block_t g_regs;
static pat_mems_sm_ctx_t g_sm;
static pat_uart5_rx_t g_u5_rx;
static pat_uart7_laser_ctx_t g_u7;
static uint8_t g_uart5_rx_byte;
static uint32_t g_last_pump_ms;

void Error_Handler(void)
{
  while (1) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(200u);
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

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef g = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  g.Pin = GPIO_PIN_0;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &g);

  g.Pin = PAT_PINMAP_SPI5_NCS_PIN;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(PAT_PINMAP_SPI5_NCS_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_SPI5_NCS_PORT, PAT_PINMAP_SPI5_NCS_PIN, GPIO_PIN_SET);

  g.Pin = PAT_PINMAP_MEMS_EN_PIN;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PAT_PINMAP_MEMS_EN_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_MEMS_EN_PORT, PAT_PINMAP_MEMS_EN_PIN, GPIO_PIN_RESET);

  g.Pin = PAT_PINMAP_LASER_DRIVER_OC_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PAT_PINMAP_LASER_DRIVER_OC_PORT, &g);

  g.Pin = PAT_PINMAP_LASER_INT_LOCK_PIN;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PAT_PINMAP_LASER_INT_LOCK_PORT, &g);
  HAL_GPIO_WritePin(PAT_PINMAP_LASER_INT_LOCK_PORT, PAT_PINMAP_LASER_INT_LOCK_PIN, GPIO_PIN_RESET);
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

static void MX_UART5_Init(void)
{
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 921600u;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  if (HAL_UART_Init(&huart5) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UART_Receive_IT(&huart5, &g_uart5_rx_byte, 1u) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_UART7_Init(void)
{
  huart7.Instance = UART7;
  huart7.Init.BaudRate = 115200u;
  huart7.Init.WordLength = UART_WORDLENGTH_8B;
  huart7.Init.StopBits = UART_STOPBITS_1;
  huart7.Init.Parity = UART_PARITY_NONE;
  huart7.Init.Mode = UART_MODE_TX_RX;
  huart7.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart7.Init.OverSampling = UART_OVERSAMPLING_16;
  huart7.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart7.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  if (HAL_UART_Init(&huart7) != HAL_OK) {
    Error_Handler();
  }

  __HAL_RCC_DMA1_CLK_ENABLE();
  hdma_uart7_rx.Instance = DMA1_Stream0;
  hdma_uart7_rx.Init.Request = DMA_REQUEST_UART7_RX;
  hdma_uart7_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma_uart7_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_uart7_rx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_uart7_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_uart7_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_uart7_rx.Init.Mode = DMA_NORMAL;
  hdma_uart7_rx.Init.Priority = DMA_PRIORITY_LOW;
  hdma_uart7_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_uart7_rx) != HAL_OK) {
    Error_Handler();
  }
  __HAL_LINKDMA(&huart7, hdmarx, hdma_uart7_rx);

  hdma_uart7_tx.Instance = DMA1_Stream1;
  hdma_uart7_tx.Init.Request = DMA_REQUEST_UART7_TX;
  hdma_uart7_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdma_uart7_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_uart7_tx.Init.MemInc = DMA_MINC_ENABLE;
  hdma_uart7_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_uart7_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma_uart7_tx.Init.Mode = DMA_NORMAL;
  hdma_uart7_tx.Init.Priority = DMA_PRIORITY_LOW;
  hdma_uart7_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdma_uart7_tx) != HAL_OK) {
    Error_Handler();
  }
  __HAL_LINKDMA(&huart7, hdmatx, hdma_uart7_tx);

  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 10u, 0u);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 10u, 0u);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
}

static void MX_SPI5_Init(void)
{
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_MASTER;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES;
  hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi5.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;
  hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 7u;
  hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi5.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi5.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi5.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi5.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi5.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi5.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi5.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi5.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi5) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_TIM_FCLK_Init(void)
{
  TIM_OC_InitTypeDef c = {0};
  uint32_t period = 5999u; /* 40kHz at 240MHz kernel. */

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0u;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = period;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0u;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
    Error_Handler();
  }
  c.OCMode = TIM_OCMODE_PWM1;
  c.Pulse = period / 2u;
  c.OCPolarity = TIM_OCPOLARITY_HIGH;
  c.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &c, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0u;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = period;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &c, TIM_CHANNEL_4) != HAL_OK) {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(TIM1_UP_IRQn, 10u, 0u);
  HAL_NVIC_SetPriority(TIM3_IRQn, 10u, 0u);
}

static void mems_pump_tick(uint32_t now_ms)
{
  uint16_t d[4];
  if ((g_regs.ctrl & PAT_MEMS_CTRL_PUMP_RUN) == 0u) {
    return;
  }
  if ((now_ms - g_last_pump_ms) < PAT_MEMS_DAC_PUMP_PERIOD_MS_DEFAULT) {
    return;
  }
  g_last_pump_ms = now_ms;
  if (pat_mems_regs_snapshot_dac4(&g_regs, d, NULL) != 0) {
    return;
  }
  (void)ad5664r_write_channel_u16(&g_dac, 0u, d[0]);
  (void)ad5664r_write_channel_u16(&g_dac, 1u, d[1]);
  (void)ad5664r_write_channel_u16(&g_dac, 2u, d[2]);
  (void)ad5664r_write_channel_u16(&g_dac, 3u, d[3]);
}

static void send_err(uint16_t seq, uint16_t origin_cmd, uint16_t err_code)
{
  pat5_frame_t tx;
  memset(&tx, 0, sizeof(tx));
  tx.ver = 1u;
  tx.flags = (uint8_t)(0x1u | 0x2u | 0x4u);
  tx.seq = seq;
  tx.cmd = origin_cmd;
  tx.len = 6u;
  tx.payload[0] = (uint8_t)(err_code & 0xFFu);
  tx.payload[1] = (uint8_t)((err_code >> 8) & 0xFFu);
  tx.payload[2] = (uint8_t)(origin_cmd & 0xFFu);
  tx.payload[3] = (uint8_t)((origin_cmd >> 8) & 0xFFu);
  tx.payload[4] = 0u;
  tx.payload[5] = 0u;
  (void)pat_uart5_send(&huart5, &tx);
}

static void dispatch_pat5(const pat5_frame_t *fr)
{
  pat5_frame_t tx;
  uint16_t n;
  uint8_t r[502];
  uint16_t rn = sizeof(r);
  memset(&tx, 0, sizeof(tx));
  tx.ver = 1u;
  tx.flags = (uint8_t)(0x1u | 0x2u);
  tx.seq = fr->seq;
  tx.cmd = fr->cmd;

  switch (fr->cmd) {
  case 0x0004u:
    if (fr->len < 2u) {
      send_err(fr->seq, fr->cmd, 0x0101u);
      return;
    }
    n = (uint16_t)((uint16_t)fr->payload[0] | ((uint16_t)fr->payload[1] << 8));
    if ((uint16_t)(n + 2u) > fr->len || n > 502u) {
      send_err(fr->seq, fr->cmd, 0x0102u);
      return;
    }
    rn = sizeof(r);
    if (pat_uart7_bypass_exchange(&g_u7, &fr->payload[2], n, r, &rn, 50u) != HAL_OK) {
      send_err(fr->seq, fr->cmd, 0x0103u);
      return;
    }
    tx.len = (uint16_t)(2u + rn);
    tx.payload[0] = (uint8_t)(rn & 0xFFu);
    tx.payload[1] = (uint8_t)((rn >> 8) & 0xFFu);
    if (rn > 0u) {
      memcpy(&tx.payload[2], r, rn);
    }
    (void)pat_uart5_send(&huart5, &tx);
    break;
  case 0x0005u:
    tx.len = (uint16_t)(8u + g_u7.cache.status_len);
    tx.payload[0] = (uint8_t)(g_u7.cache.last_poll_ms & 0xFFu);
    tx.payload[1] = (uint8_t)((g_u7.cache.last_poll_ms >> 8) & 0xFFu);
    tx.payload[2] = (uint8_t)((g_u7.cache.last_poll_ms >> 16) & 0xFFu);
    tx.payload[3] = (uint8_t)((g_u7.cache.last_poll_ms >> 24) & 0xFFu);
    tx.payload[4] = (uint8_t)(g_u7.cache.status_len & 0xFFu);
    tx.payload[5] = (uint8_t)((g_u7.cache.status_len >> 8) & 0xFFu);
    tx.payload[6] = (uint8_t)(g_u7.cache.cache_flags & 0xFFu);
    tx.payload[7] = (uint8_t)((g_u7.cache.cache_flags >> 8) & 0xFFu);
    if (g_u7.cache.status_len > 0u) {
      memcpy(&tx.payload[8], g_u7.cache.status_blob, g_u7.cache.status_len);
    }
    (void)pat_uart5_send(&huart5, &tx);
    break;
  case 0x0200u:
    if (fr->len != 32u) {
      send_err(fr->seq, fr->cmd, 0x0201u);
      return;
    }
    memcpy(&g_regs, fr->payload, 32u);
    tx.len = 0u;
    (void)pat_uart5_send(&huart5, &tx);
    break;
  case 0x0201u:
    if (fr->len != 4u) {
      send_err(fr->seq, fr->cmd, 0x0202u);
      return;
    }
    g_regs.fc_hz = ((uint32_t)fr->payload[0] | ((uint32_t)fr->payload[1] << 8) |
                    ((uint32_t)fr->payload[2] << 16) | ((uint32_t)fr->payload[3] << 24));
    tx.len = 0u;
    (void)pat_uart5_send(&huart5, &tx);
    break;
  case 0x0202u:
    if (fr->len != 4u) {
      send_err(fr->seq, fr->cmd, 0x0203u);
      return;
    }
    g_regs.ctrl = ((uint32_t)fr->payload[0] | ((uint32_t)fr->payload[1] << 8) |
                   ((uint32_t)fr->payload[2] << 16) | ((uint32_t)fr->payload[3] << 24));
    tx.len = 0u;
    (void)pat_uart5_send(&huart5, &tx);
    break;
  case 0x0203u:
    tx.len = 6u;
    tx.payload[0] = (uint8_t)pat_mems_sm_state(&g_sm);
    tx.payload[1] = 0u;
    tx.payload[2] = (uint8_t)(g_u5_rx.crc_fail & 0xFFu);
    tx.payload[3] = (uint8_t)((g_u5_rx.crc_fail >> 8) & 0xFFu);
    tx.payload[4] = (uint8_t)(g_u5_rx.len_fail & 0xFFu);
    tx.payload[5] = (uint8_t)((g_u5_rx.len_fail >> 8) & 0xFFu);
    (void)pat_uart5_send(&huart5, &tx);
    break;
  default:
    send_err(fr->seq, fr->cmd, 0xFFFFu);
    break;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart5) {
    pat_uart5_rx_push(&g_u5_rx, &g_uart5_rx_byte, 1u);
    (void)HAL_UART_Receive_IT(&huart5, &g_uart5_rx_byte, 1u);
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart == &huart7) {
    pat_uart7_on_rx_event(&g_u7, size);
    (void)pat_uart7_start_rx_dma(&g_u7);
  }
}

int main(void)
{
  pat5_frame_t fr;
  uint32_t now;

  HAL_Init();
  PAT_SystemClock_Config();
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_SPI5_Init();
  MX_UART5_Init();
  MX_UART7_Init();
  MX_TIM_FCLK_Init();
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("\r\nPAT: MEMS bringup SPI5/UART5/UART7 start\r\n");

  ad5664r_init_dev(&g_dac, &hspi5, PAT_PINMAP_SPI5_NCS_PORT, PAT_PINMAP_SPI5_NCS_PIN);
  pat_mems_regs_init(&g_regs);
  memset(&g_sm, 0, sizeof(g_sm));
  g_sm.regs = &g_regs;
  g_sm.dac = &g_dac;
  g_sm.htim_x = &htim3;
  g_sm.tim_x_ch = TIM_CHANNEL_4;
  g_sm.htim_y = &htim1;
  g_sm.tim_y_ch = TIM_CHANNEL_1;
  g_sm.en_port = PAT_PINMAP_MEMS_EN_PORT;
  g_sm.en_pin = PAT_PINMAP_MEMS_EN_PIN;
  pat_mems_sm_init(&g_sm);
  pat_uart5_rx_init(&g_u5_rx);
  pat_uart7_laser_init(&g_u7, &huart7, &hdma_uart7_rx, &hdma_uart7_tx);
  (void)pat_uart7_start_rx_dma(&g_u7);

  for (;;) {
    now = HAL_GetTick();
    pat_uart7_poll_parser(&g_u7);
    (void)pat_uart7_status_tick(&g_u7, now, 1000u);
    while (pat_uart5_try_parse(&g_u5_rx, &fr) == 1) {
      dispatch_pat5(&fr);
    }
    pat_mems_sm_poll(&g_sm, now);
    mems_pump_tick(now);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
    HAL_Delay(100u);
  }
}

void UART5_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart5);
}

void UART7_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart7);
}

void DMA1_Stream0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_uart7_rx);
}

void DMA1_Stream1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_uart7_tx);
}
