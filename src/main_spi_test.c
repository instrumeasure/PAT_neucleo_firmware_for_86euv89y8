#include <stdio.h>
#include "stm32h7xx_hal.h"
#include "pat_clock.h"
#include "pat_pinmap.h"
#include "ads127l11.h"
#include "ads127l11_hal_stm32.h"

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;
SPI_HandleTypeDef hspi4;
UART_HandleTypeDef huart3;

static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI4_Init(void);
static void MX_USART3_UART_Init(void);
static void bind_ads127_interfaces(void);
void Error_Handler(void);

int _write(int file, char *ptr, int len)
{
    (void)file;
    if (HAL_UART_Transmit(&huart3, (uint8_t *)ptr, (uint16_t)len, 50U) == HAL_OK)
    {
        return len;
    }
    return 0;
}

int main(void)
{
    uint32_t i;
    uint32_t now;
    uint32_t next_hb_ms = 0U;
    ads127_device_t ads[ADS127_SYNC_CHANNELS];

    HAL_Init();
    PAT_SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_SPI3_Init();
    MX_SPI4_Init();
    MX_USART3_UART_Init();
    bind_ads127_interfaces();

    for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
    {
        ads127_init_device(&ads[i], (uint8_t)i);
    }

    printf("BOOT,SPI_TEST_ONLY,%lu\r\n", (unsigned long)HAL_GetTick());
    printf("SPI_VERIFY,mode,boot_reset_then_poll_DEV_REV_only,uart,USART3_115200\r\n");
    printf("SPI_VERIFY,sclk_prescaler,%u\r\n", (unsigned)hspi1.Init.BaudRatePrescaler);
    printf("SPI_VERIFY,pins,ch0,SPI1,CS_PA4,MISO_PG9,MOSI_PD7,SCK_PG11,START_PF1,RESET_PF0\r\n");

    /* One full SBAS946 reset + verify per channel; avoids violating td(RSSC) by hammering nRESET every second. */
    for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
    {
        bool ok = ads127_spi_verify_link(&ads[i]);
        printf("SPI_INIT,ch%u,%s,DEV,%02X,REV,%02X\r\n",
               (unsigned)i,
               ok ? "OK" : "FAIL",
               (unsigned)ads[i].dev_id_hw,
               (unsigned)ads[i].rev_id_hw);
    }

    for (;;)
    {
        uint32_t hal_cmd;
        uint32_t hal_nop;
        bool ok;

        now = HAL_GetTick();
        if (now < next_hb_ms)
        {
            HAL_Delay(5U);
            continue;
        }
        next_hb_ms = now + 1000U;

        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7);
        printf("HB,SPI_TEST,%lu\r\n", (unsigned long)now);

        for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
        {
            hal_cmd = 0U;
            hal_nop = 0U;
            ok = ads127_spi_poll_dev_rev(&ads[i], &hal_cmd, &hal_nop);
            printf("SPI_POLL,ch%u,%s,DEV,%02X,REV,%02X,HAL_CMD,%lu,HAL_NOP,%lu\r\n",
                   (unsigned)i,
                   ok ? "OK" : "FAIL",
                   (unsigned)ads[i].dev_id_hw,
                   (unsigned)ads[i].rev_id_hw,
                   (unsigned long)hal_cmd,
                   (unsigned long)hal_nop);
        }
    }
}

static void bind_ads127_interfaces(void)
{
    ADS127_HAL_BindSpi(0U, &hspi1);
    ADS127_HAL_BindSpi(1U, &hspi2);
    ADS127_HAL_BindSpi(2U, &hspi3);
    ADS127_HAL_BindSpi(3U, &hspi4);

    ADS127_HAL_SetCsPin(0U, PAT_PINMAP_SPI1_NCS_PORT, PAT_PINMAP_SPI1_NCS_PIN);
    ADS127_HAL_SetCsPin(1U, PAT_PINMAP_SPI2_NCS_PORT, PAT_PINMAP_SPI2_NCS_PIN);
    ADS127_HAL_SetCsPin(2U, PAT_PINMAP_SPI3_NCS_PORT, PAT_PINMAP_SPI3_NCS_PIN);
    ADS127_HAL_SetCsPin(3U, PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN);
    ADS127_HAL_SetStartPin(PAT_PINMAP_ADS127_START_PORT, PAT_PINMAP_ADS127_START_PIN);
    ADS127_HAL_SetResetPin(PAT_PINMAP_ADS127_NRESET_PORT, PAT_PINMAP_ADS127_NRESET_PIN);
}

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    /* Slowest standard divider: prioritise margin on SI / harness over throughput until MISO is valid. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI2_Init(void)
{
    hspi2 = hspi1;
    hspi2.Instance = SPI2;
    hspi2.State = HAL_SPI_STATE_RESET;
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI3_Init(void)
{
    hspi3 = hspi1;
    hspi3.Instance = SPI3;
    hspi3.State = HAL_SPI_STATE_RESET;
    if (HAL_SPI_Init(&hspi3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI4_Init(void)
{
    hspi4 = hspi1;
    hspi4.Instance = SPI4;
    hspi4.State = HAL_SPI_STATE_RESET;
    if (HAL_SPI_Init(&hspi4) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    HAL_GPIO_WritePin(PAT_PINMAP_SPI1_NCS_PORT,
                      PAT_PINMAP_SPI1_NCS_PIN | PAT_PINMAP_SPI3_NCS_PIN,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(PAT_PINMAP_SPI2_NCS_PORT, PAT_PINMAP_SPI2_NCS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PAT_PINMAP_SPI4_NCS_PORT, PAT_PINMAP_SPI4_NCS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(PAT_PINMAP_ADS127_NRESET_PORT,
                      PAT_PINMAP_ADS127_NRESET_PIN | PAT_PINMAP_ADS127_START_PIN,
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = PAT_PINMAP_SPI1_NCS_PIN | PAT_PINMAP_SPI3_NCS_PIN;
    HAL_GPIO_Init(PAT_PINMAP_SPI1_NCS_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PAT_PINMAP_SPI2_NCS_PIN | GPIO_PIN_0 | GPIO_PIN_7;
    HAL_GPIO_Init(PAT_PINMAP_SPI2_NCS_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PAT_PINMAP_SPI4_NCS_PIN;
    HAL_GPIO_Init(PAT_PINMAP_SPI4_NCS_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = PAT_PINMAP_ADS127_NRESET_PIN | PAT_PINMAP_ADS127_START_PIN;
    HAL_GPIO_Init(PAT_PINMAP_ADS127_NRESET_PORT, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7);
        HAL_Delay(200U);
    }
}
