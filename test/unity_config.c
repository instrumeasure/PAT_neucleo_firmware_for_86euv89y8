/*
 Copyright (c) 2014-present PlatformIO

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 NUCLEO-H753ZI: USART3 on PD8 (TX) / PD9 (RX), AF7 — ST-Link virtual COM (same as application firmware).
**/

#include "unity_config.h"
#include "stm32h7xx_hal.h"

static UART_HandleTypeDef s_unity_uart;
static uint8_t s_board_ready;

static void unity_clock_init(void)
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
        while (1)
        {
        }
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
        while (1)
        {
        }
    }

    SystemCoreClockUpdate();

    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
    {
        while (1)
        {
        }
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();

        __HAL_RCC_GPIOD_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    }
}

void unityOutputStart(void)
{
    if (s_board_ready)
    {
        return;
    }
    s_board_ready = 1;

    HAL_Init();
    unity_clock_init();

    s_unity_uart.Instance = USART3;
    s_unity_uart.Init.BaudRate = 115200;
    s_unity_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_unity_uart.Init.StopBits = UART_STOPBITS_1;
    s_unity_uart.Init.Parity = UART_PARITY_NONE;
    s_unity_uart.Init.Mode = UART_MODE_TX_RX;
    s_unity_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_unity_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    s_unity_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_unity_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&s_unity_uart) != HAL_OK)
    {
        while (1)
        {
        }
    }
}

void unityOutputChar(char c)
{
    (void)HAL_UART_Transmit(&s_unity_uart, (uint8_t *)&c, 1U, 1000U);
}

void unityOutputFlush(void)
{
}

void unityOutputComplete(void)
{
}
