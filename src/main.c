#include <stdio.h>
#include "stm32h7xx_hal.h"
#include "ads127l11.h"
#include "ads127l11_hal_stm32.h"
#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
#include "app_state.h"
#include "qpd_dsp.h"
#include "qpd_spi6_slave.h"
#endif

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;
SPI_HandleTypeDef hspi4;
SPI_HandleTypeDef hspi6;
UART_HandleTypeDef huart3;
TIM_HandleTypeDef htim6;

#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
/*
 * Quartet gate — TIM6 vs nominal ODR (bring-up leaves CONFIG at POR; macro matches expected profile).
 * ADS127_CONFIG4_USER bit7: 25 MHz ext path ~48.8 k; internal 25.6 MHz nominal 50 k (see ads127l11.h).
 */
#if (ADS127_CONFIG4_USER & 0x80U) != 0U
#define SAMPLE_RATE_HZ ADS127_ODR_HZ_25M_EXT
#else
#define SAMPLE_RATE_HZ ADS127_ODR_HZ_NOMINAL_25M6_MHZ
#endif

volatile uint32_t g_adc_tick_pending;

static app_state_t g_state = APP_STATE_INIT;
static ads127_device_t g_ads[ADS127_SYNC_CHANNELS];
static int32_t g_last_raw[ADS127_SYNC_CHANNELS] = {
    ADS127_RAW_INVALID,
    ADS127_RAW_INVALID,
    ADS127_RAW_INVALID,
    ADS127_RAW_INVALID};
#endif /* !PAT_SPI_VERIFY_ONLY */

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI4_Init(void);
static void MX_USART3_UART_Init(void);
static void Error_Handler(void);
static void state_led_write(GPIO_PinState level);
#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
static bool adc_sample_gate_take_one(void);
static void MX_TIM6_SampleGate_Init(void);
static void MX_SPI6_Init(void);
static void heartbeat_tick(void);
static void state_led_tick(void);
static void early_led_sanity_sweep(void);
static void MX_Reassert_UserLeds_AsOutputs(void);
#else
static void run_spi_verify_only(void);
#endif

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
#if defined(PAT_SPI_VERIFY_ONLY) && PAT_SPI_VERIFY_ONLY
    HAL_Init();
    SystemClock_Config();
    run_spi_verify_only();
    while (1)
    {
    }
#else
    uint32_t i;
    bool startup_ok = true;
    ads127_sample_set_t sample_set;
    qpd_dsp_output_t dsp_out;

    HAL_Init();
    early_led_sanity_sweep();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_SPI3_Init();
    MX_SPI4_Init();
    MX_USART3_UART_Init();
    MX_SPI6_Init();
    MX_Reassert_UserLeds_AsOutputs();
    MX_TIM6_SampleGate_Init();

    ADS127_HAL_BindSpi(0U, &hspi1);
    ADS127_HAL_BindSpi(1U, &hspi2);
    ADS127_HAL_BindSpi(2U, &hspi3);
    ADS127_HAL_BindSpi(3U, &hspi4);

    ADS127_HAL_SetCsPin(0U, GPIOA, GPIO_PIN_4);
    ADS127_HAL_SetCsPin(1U, GPIOB, GPIO_PIN_4);
    ADS127_HAL_SetCsPin(2U, GPIOA, GPIO_PIN_15);
    ADS127_HAL_SetCsPin(3U, GPIOE, GPIO_PIN_11);
    ADS127_HAL_SetStartPin(GPIOF, GPIO_PIN_1);
    ADS127_HAL_SetResetPin(GPIOF, GPIO_PIN_0);

    ADS127_HAL_SetRESET(true);
    ADS127_HAL_SetSTART(true);

    for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
    {
        ads127_init_device(&g_ads[i], (uint8_t)i);
    }

    g_state = APP_STATE_ADC_CFG;
    for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
    {
        if (!ads127_startup(&g_ads[i]))
        {
            startup_ok = false;
            break;
        }
    }

    if (startup_ok)
    {
        g_state = APP_STATE_RUN;
    }
    else
    {
        g_state = APP_STATE_ERR_ADC;
    }

    qpd_dsp_init();
    qpd_spi6_slave_init(&hspi6);

    printf("BOOT,%s,%lu\r\n", app_state_to_string(g_state), HAL_GetTick());
    for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
    {
        printf("ADC_SPI,ch%u,DEV,%02X,REV,%02X,ST,%02X\r\n",
               (unsigned)i,
               (unsigned)g_ads[i].dev_id_hw,
               (unsigned)g_ads[i].rev_id_hw,
               (unsigned)g_ads[i].status_hw);
    }

    if (g_state == APP_STATE_RUN)
    {
        g_adc_tick_pending = 0U;
        if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK)
        {
            g_state = APP_STATE_ERR_SPI;
        }
    }

    while (1)
    {
        if (g_state == APP_STATE_RUN && adc_sample_gate_take_one())
        {
            if (ads127_read_synchronous_quartet(g_ads, &sample_set))
            {
                for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
                {
                    g_last_raw[i] = sample_set.raw[i];
                }
                qpd_dsp_on_quartet(&sample_set, &dsp_out);
                qpd_spi6_slave_pack_latest(&sample_set, &dsp_out);
            }
            else
            {
                g_state = APP_STATE_ERR_SPI;
                for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
                {
                    g_last_raw[i] = ADS127_RAW_INVALID;
                }
            }
        }

        state_led_tick();
        heartbeat_tick();
    }
#endif /* !PAT_SPI_VERIFY_ONLY */
}

#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
static bool adc_sample_gate_take_one(void)
{
    bool take = false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (g_adc_tick_pending != 0U)
    {
        g_adc_tick_pending--;
        take = true;
    }
    __set_PRIMASK(primask);

    return take;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM6)
    {
        return;
    }

    /* If main falls behind, cap backlog so heartbeat/LED remain serviced. */
    if (g_adc_tick_pending < 8U)
    {
        g_adc_tick_pending++;
    }
}

void HAL_TIM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_CLK_ENABLE();
    }
}

static void MX_TIM6_SampleGate_Init(void)
{
    uint32_t timclk_hz;
    uint32_t arr;

    /*
     * TIM6 on APB1 (D2). If D2PPRE1 != DIV1, timer kernel clock is 2 * PCLK1 (RM0433 RCC).
     */
    timclk_hz = HAL_RCC_GetPCLK1Freq();
    if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE1) != 0U)
    {
        timclk_hz *= 2U;
    }

    /*
     * PSC=0, ARR = round(timclk / SAMPLE_RATE_HZ) - 1.
     * If TIM clock is wrong in your RCC tree, verify with the debugger (TIM counter vs scope).
     */
    arr = (timclk_hz / SAMPLE_RATE_HZ);
    if (arr == 0U)
    {
        arr = 1U;
    }
    arr--;

    if (arr > 0xFFFFU)
    {
        /* Would need PSC > 0 — not expected at 50 kHz on typical H753 clocks */
        Error_Handler();
    }

    htim6.Instance = TIM6;
    htim6.Init.Prescaler = 0;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = (uint16_t)arr;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

static void heartbeat_tick(void)
{
    uint32_t now;
    static uint32_t next_hb_ms = 0U;
    uint32_t raw0;
    uint32_t raw1;
    uint32_t raw2;
    uint32_t raw3;

    now = HAL_GetTick();
    if (now < next_hb_ms)
    {
        return;
    }
    next_hb_ms = now + 1000U;

    raw0 = (g_last_raw[0] == ADS127_RAW_INVALID) ? 0xFFFFFFUL : ((uint32_t)g_last_raw[0] & 0xFFFFFFUL);
    raw1 = (g_last_raw[1] == ADS127_RAW_INVALID) ? 0xFFFFFFUL : ((uint32_t)g_last_raw[1] & 0xFFFFFFUL);
    raw2 = (g_last_raw[2] == ADS127_RAW_INVALID) ? 0xFFFFFFUL : ((uint32_t)g_last_raw[2] & 0xFFFFFFUL);
    raw3 = (g_last_raw[3] == ADS127_RAW_INVALID) ? 0xFFFFFFUL : ((uint32_t)g_last_raw[3] & 0xFFFFFFUL);

    printf("HB,%s,%lu,%lu,0x%06lX,0x%06lX,0x%06lX,0x%06lX\r\n",
           app_state_to_string(g_state),
           now,
           (unsigned long)ads127_get_quartet_acquired_count(),
           raw0,
           raw1,
           raw2,
           raw3);
    /* Live RREG on DEV_ID / REV_ID / STATUS every heartbeat for SPI verification (SBAS946). */
    {
        uint32_t c;
        uint8_t v;

        for (c = 0U; c < ADS127_SYNC_CHANNELS; c++)
        {
            if (!ads127_read_register(&g_ads[c], ADS127_REG_DEV_ID, &v, 0, 0))
            {
                g_ads[c].dev_id_hw = 0xFFU;
            }
            else
            {
                g_ads[c].dev_id_hw = v;
            }
            if (!ads127_read_register(&g_ads[c], ADS127_REG_REV_ID, &v, 0, 0))
            {
                g_ads[c].rev_id_hw = 0xFFU;
            }
            else
            {
                g_ads[c].rev_id_hw = v;
            }
            if (!ads127_read_register(&g_ads[c], ADS127_REG_STATUS, &v, 0, 0))
            {
                g_ads[c].status_hw = 0xFFU;
            }
            else
            {
                g_ads[c].status_hw = v;
            }
        }
    }
    printf("ADC_SPI_IDS,ch0,DEV,%02X,REV,%02X,ST,%02X,ch1,DEV,%02X,REV,%02X,ST,%02X,ch2,DEV,%02X,REV,%02X,ST,%02X,ch3,DEV,%02X,REV,%02X,ST,%02X\r\n",
           (unsigned)g_ads[0].dev_id_hw,
           (unsigned)g_ads[0].rev_id_hw,
           (unsigned)g_ads[0].status_hw,
           (unsigned)g_ads[1].dev_id_hw,
           (unsigned)g_ads[1].rev_id_hw,
           (unsigned)g_ads[1].status_hw,
           (unsigned)g_ads[2].dev_id_hw,
           (unsigned)g_ads[2].rev_id_hw,
           (unsigned)g_ads[2].status_hw,
           (unsigned)g_ads[3].dev_id_hw,
           (unsigned)g_ads[3].rev_id_hw,
           (unsigned)g_ads[3].status_hw);

    /*
     * After ERR_ADC the main loop does not read the quartet, so SPI is otherwise idle.
     * Short transfers here give a repeating scope pattern (CS + SCLK on each bus).
     */
    if (g_state == APP_STATE_ERR_ADC)
    {
        uint8_t tx[3] = {0U, 0U, 0U};
        uint8_t rx[3];
        uint32_t c;

        for (c = 0U; c < ADS127_SYNC_CHANNELS; c++)
        {
            (void)ADS127_HAL_SPI_Transfer((uint8_t)c, tx, rx, 3U, 10U);
        }
    }
}

static void state_led_tick(void)
{
    static uint32_t next_ms = 0U;
    static uint8_t phase = 0U;
    uint32_t now = HAL_GetTick();
    uint32_t interval_ms = 250U;
    GPIO_PinState level = GPIO_PIN_RESET;

    if (now < next_ms)
    {
        return;
    }

    switch (g_state)
    {
    case APP_STATE_INIT:
        interval_ms = 150U;
        level = (phase & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        phase++;
        break;
    case APP_STATE_ADC_CFG:
        interval_ms = 80U;
        level = (phase & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        phase++;
        break;
    case APP_STATE_RUN:
        interval_ms = 500U;
        level = (phase & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        phase++;
        break;
    case APP_STATE_ERR_SPI:
        interval_ms = (phase == 0U) ? 120U : 880U;
        level = (phase == 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        phase = (phase + 1U) % 2U;
        break;
    case APP_STATE_ERR_ADC:
    default:
        interval_ms = 500U;
        level = (phase & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        phase++;
        break;
    }

    state_led_write(level);
    next_ms = now + interval_ms;
}

#endif /* !PAT_SPI_VERIFY_ONLY */

static void state_led_write(GPIO_PinState level)
{
    /* LD1=PB0, LD2=PB7. PB14 is SPI2_MISO (CubeMX); LD3 on PB14 is not used. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, level);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, level);
}

#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
static void early_led_sanity_sweep(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint32_t i;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    for (i = 0U; i < 8U; i++)
    {
        GPIO_PinState level = (i & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, level);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, level);
        HAL_Delay(120U);
    }

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET);
}

#endif /* !PAT_SPI_VERIFY_ONLY — early_led_sanity_sweep */

static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    /* SCLK = f_spi_ker / prescaler; slower SPI eases SI / long leads vs ADS127 (was ÷32). */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
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
    /* ST CubeH7 SPI examples recommend ENABLE to avoid output glitches on H7 SPI masters. */
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
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI3_Init(void)
{
    hspi3 = hspi1;
    hspi3.Instance = SPI3;
    if (HAL_SPI_Init(&hspi3) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_SPI4_Init(void)
{
    hspi4 = hspi1;
    hspi4.Instance = SPI4;
    if (HAL_SPI_Init(&hspi4) != HAL_OK)
    {
        Error_Handler();
    }
}

#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
static void MX_SPI6_Init(void)
{
    hspi6.Instance = SPI6;
    hspi6.Init.Mode = SPI_MODE_SLAVE;
    hspi6.Init.Direction = SPI_DIRECTION_2LINES;
    hspi6.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi6.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi6.Init.CLKPhase = SPI_PHASE_2EDGE;
    hspi6.Init.NSS = SPI_NSS_HARD_INPUT;
    hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi6.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi6.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi6.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi6.Init.CRCPolynomial = 7U;
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

    if (HAL_SPI_Init(&hspi6) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(SPI6_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(SPI6_IRQn);
}

#endif /* !PAT_SPI_VERIFY_ONLY */

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

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_15, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);
    state_led_write(GPIO_PIN_RESET);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* LD1=PB0, LD2=PB7 (PB14 reserved for SPI2 MISO). */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

#if !defined(PAT_SPI_VERIFY_ONLY) || !PAT_SPI_VERIFY_ONLY
static void MX_Reassert_UserLeds_AsOutputs(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Do not touch PB10/PB14/PB15 — SPI2 AF after MX_SPI2_Init. */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

#endif /* !PAT_SPI_VERIFY_ONLY */

#if defined(PAT_SPI_VERIFY_ONLY) && PAT_SPI_VERIFY_ONLY
static void run_spi_verify_only(void)
{
    uint32_t i;
    ads127_device_t ads[ADS127_SYNC_CHANNELS];

    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_SPI3_Init();
    MX_SPI4_Init();
    MX_USART3_UART_Init();

    ADS127_HAL_BindSpi(0U, &hspi1);
    ADS127_HAL_BindSpi(1U, &hspi2);
    ADS127_HAL_BindSpi(2U, &hspi3);
    ADS127_HAL_BindSpi(3U, &hspi4);

    ADS127_HAL_SetCsPin(0U, GPIOA, GPIO_PIN_4);
    ADS127_HAL_SetCsPin(1U, GPIOB, GPIO_PIN_4);
    ADS127_HAL_SetCsPin(2U, GPIOA, GPIO_PIN_15);
    ADS127_HAL_SetCsPin(3U, GPIOE, GPIO_PIN_11);
    ADS127_HAL_SetStartPin(GPIOF, GPIO_PIN_1);
    ADS127_HAL_SetResetPin(GPIOF, GPIO_PIN_0);

    for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
    {
        ads127_init_device(&ads[i], (uint8_t)i);
    }

    printf("SPI_VERIFY,reset,DEV_ID expect 0x%02X\r\n", (unsigned)ADS127_DEV_ID_EXPECTED);

    for (;;)
    {
        for (i = 0U; i < ADS127_SYNC_CHANNELS; i++)
        {
            bool ok = ads127_spi_verify_link(&ads[i]);
            printf("SPI_VERIFY,ch%u,%s,DEV,%02X,REV,%02X\r\n",
                   (unsigned)i,
                   ok ? "OK" : "FAIL",
                   (unsigned)ads[i].dev_id_hw,
                   (unsigned)ads[i].rev_id_hw);
        }
        HAL_Delay(500U);
    }
}
#endif

void HAL_SPI_MspInit(SPI_HandleTypeDef *spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (spiHandle->Instance == SPI1)
    {
        __HAL_RCC_SPI1_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_11;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    }
    else if (spiHandle->Instance == SPI2)
    {
        /* PB10 SCK, PB15 MOSI, PB14 MISO — matches STM32CubeMX PAT pinout. */
        __HAL_RCC_SPI2_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
    else if (spiHandle->Instance == SPI3)
    {
        __HAL_RCC_SPI3_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_6;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI3;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    }
    else if (spiHandle->Instance == SPI4)
    {
        __HAL_RCC_SPI4_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_12 | GPIO_PIN_13;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI4;
        HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    }
    else if (spiHandle->Instance == SPI6)
    {
        /* Inter-HAT J2: PA5 SCK, PG8 NSS, PG12 MISO, PG14 MOSI — AF8 (RM0433). */
        __HAL_RCC_SPI6_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_5;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_SPI6;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_12 | GPIO_PIN_14;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 4;
    RCC_OscInitStruct.PLL.PLLN = 50;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    SystemCoreClockUpdate();

    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
    {
        Error_Handler();
    }
}

static void Error_Handler(void)
{
#if defined(PAT_SPI_VERIFY_ONLY) && PAT_SPI_VERIFY_ONLY
    while (1)
    {
        HAL_Delay(250U);
    }
#else
    /* Ensure LED pins are usable even if init failed early. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    g_state = APP_STATE_ERR_SPI;
    while (1)
    {
        state_led_tick();
        heartbeat_tick();
        HAL_Delay(10U);
    }
#endif
}
