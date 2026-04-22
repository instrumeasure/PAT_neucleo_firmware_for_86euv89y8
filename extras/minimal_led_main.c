#include "stm32h7xx_hal.h"

static void MX_GPIO_Init(void);
static void SystemClock_Config(void);
static void Error_Handler(void);
static void set_all_leds(GPIO_PinState level);
static void pulse_pin(GPIO_TypeDef *port, uint16_t pin, uint32_t on_ms, uint32_t off_ms);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    while (1)
    {
        /* Per-pin sweep for visual identification on board revision. */
        pulse_pin(GPIOB, GPIO_PIN_0, 400U, 400U);   /* LD1 (H753ZI BSP) */
        pulse_pin(GPIOB, GPIO_PIN_7, 400U, 400U);   /* LD2 (H753ZI BSP) */
        pulse_pin(GPIOB, GPIO_PIN_14, 400U, 400U);  /* LD3 (H753ZI BSP) */
        pulse_pin(GPIOE, GPIO_PIN_1, 400U, 400U);   /* Alternate LD2 on some variants */

        /* Group blink (easy to spot). */
        set_all_leds(GPIO_PIN_SET);
        HAL_Delay(500U);
        set_all_leds(GPIO_PIN_RESET);
        HAL_Delay(500U);
    }
}

static void set_all_leds(GPIO_PinState level)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_14, level);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, level);
}

static void pulse_pin(GPIO_TypeDef *port, uint16_t pin, uint32_t on_ms, uint32_t off_ms)
{
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    HAL_Delay(on_ms);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    HAL_Delay(off_ms);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    set_all_leds(GPIO_PIN_RESET);
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

    /* HAL_Init() set SysTick for the reset-clock rate; reload after PLL. */
    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
    {
        Error_Handler();
    }
}

static void Error_Handler(void)
{
    while (1)
    {
    }
}
