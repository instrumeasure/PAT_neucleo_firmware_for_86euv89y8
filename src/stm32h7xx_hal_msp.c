#include "stm32h7xx_hal.h"
#include "pat_pinmap.h"

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  if (hspi->Instance == SPI1) {
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Pin = PAT_PINMAP_SPI1_MOSI_PIN;
    gpio.Alternate = PAT_PINMAP_SPI1_MOSI_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI1_MOSI_PORT, &gpio);

    gpio.Pin = PAT_PINMAP_SPI1_MISO_PIN | PAT_PINMAP_SPI1_SCK_PIN;
    gpio.Alternate = PAT_PINMAP_SPI1_SCK_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI1_SCK_PORT, &gpio);
  } else if (hspi->Instance == SPI2) {
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = PAT_PINMAP_SPI2_SCK_PIN | PAT_PINMAP_SPI2_MOSI_PIN;
    gpio.Alternate = PAT_PINMAP_SPI2_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI2_SCK_PORT, &gpio);

    gpio.Pin = PAT_PINMAP_SPI2_MISO_PIN;
    gpio.Alternate = PAT_PINMAP_SPI2_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI2_MISO_PORT, &gpio);
  } else if (hspi->Instance == SPI3) {
    __HAL_RCC_SPI3_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    gpio.Pin = PAT_PINMAP_SPI3_SCK_PIN | PAT_PINMAP_SPI3_MISO_PIN;
    gpio.Alternate = PAT_PINMAP_SPI3_SCK_MISO_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI3_SCK_PORT, &gpio);

    gpio.Pin = PAT_PINMAP_SPI3_MOSI_PIN;
    gpio.Alternate = PAT_PINMAP_SPI3_MOSI_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI3_MOSI_PORT, &gpio);
  } else if (hspi->Instance == SPI4) {
    __HAL_RCC_SPI4_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio.Pin = PAT_PINMAP_SPI4_AF_PINS;
    gpio.Alternate = PAT_PINMAP_SPI4_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI4_SCK_PORT, &gpio);
  } else if (hspi->Instance == SPI6) {
    __HAL_RCC_SPI6_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Pin = PAT_PINMAP_SPI6_SCK_PIN;
    gpio.Alternate = PAT_PINMAP_SPI6_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI6_SCK_PORT, &gpio);

    gpio.Pin = PAT_PINMAP_SPI6_GPIOG_AF_PINS;
    gpio.Alternate = PAT_PINMAP_SPI6_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI6_NSS_PORT, &gpio);
  } else if (hspi->Instance == SPI5) {
    __HAL_RCC_SPI5_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    gpio.Pin = PAT_PINMAP_SPI5_AF_PINS;
    gpio.Alternate = PAT_PINMAP_SPI5_AF;
    HAL_GPIO_Init(PAT_PINMAP_SPI5_SCK_PORT, &gpio);
  }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1) {
    HAL_GPIO_DeInit(PAT_PINMAP_SPI1_MOSI_PORT, PAT_PINMAP_SPI1_MOSI_PIN);
    HAL_GPIO_DeInit(PAT_PINMAP_SPI1_SCK_PORT,
                    PAT_PINMAP_SPI1_SCK_PIN | PAT_PINMAP_SPI1_MISO_PIN);
    __HAL_RCC_SPI1_CLK_DISABLE();
  } else if (hspi->Instance == SPI2) {
    HAL_GPIO_DeInit(PAT_PINMAP_SPI2_SCK_PORT,
                    PAT_PINMAP_SPI2_SCK_PIN | PAT_PINMAP_SPI2_MOSI_PIN);
    HAL_GPIO_DeInit(PAT_PINMAP_SPI2_MISO_PORT, PAT_PINMAP_SPI2_MISO_PIN);
    __HAL_RCC_SPI2_CLK_DISABLE();
  } else if (hspi->Instance == SPI3) {
    HAL_GPIO_DeInit(PAT_PINMAP_SPI3_SCK_PORT,
                    PAT_PINMAP_SPI3_SCK_PIN | PAT_PINMAP_SPI3_MISO_PIN);
    HAL_GPIO_DeInit(PAT_PINMAP_SPI3_MOSI_PORT, PAT_PINMAP_SPI3_MOSI_PIN);
    __HAL_RCC_SPI3_CLK_DISABLE();
  } else if (hspi->Instance == SPI4) {
    HAL_GPIO_DeInit(PAT_PINMAP_SPI4_SCK_PORT, PAT_PINMAP_SPI4_AF_PINS);
    __HAL_RCC_SPI4_CLK_DISABLE();
  } else if (hspi->Instance == SPI6) {
    HAL_GPIO_DeInit(PAT_PINMAP_SPI6_SCK_PORT, PAT_PINMAP_SPI6_SCK_PIN);
    HAL_GPIO_DeInit(PAT_PINMAP_SPI6_NSS_PORT, PAT_PINMAP_SPI6_GPIOG_AF_PINS);
    __HAL_RCC_SPI6_CLK_DISABLE();
  } else if (hspi->Instance == SPI5) {
    HAL_GPIO_DeInit(PAT_PINMAP_SPI5_SCK_PORT, PAT_PINMAP_SPI5_AF_PINS);
    __HAL_RCC_SPI5_CLK_DISABLE();
  }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  if (huart->Instance == USART3) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    gpio.Alternate = GPIO_AF7_USART3;
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOD, &gpio);
    HAL_NVIC_SetPriority(USART3_IRQn, 12u, 0u);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  } else if (huart->Instance == UART5) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_UART5_CLK_ENABLE();
    gpio.Alternate = PAT_PINMAP_UART5_AF;
    gpio.Pin = PAT_PINMAP_UART5_TX_PIN;
    HAL_GPIO_Init(PAT_PINMAP_UART5_TX_PORT, &gpio);
    gpio.Pin = PAT_PINMAP_UART5_RX_PIN;
    HAL_GPIO_Init(PAT_PINMAP_UART5_RX_PORT, &gpio);
    HAL_NVIC_SetPriority(UART5_IRQn, 10u, 0u);
    HAL_NVIC_EnableIRQ(UART5_IRQn);
  } else if (huart->Instance == UART7) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_UART7_CLK_ENABLE();
    gpio.Alternate = PAT_PINMAP_UART7_AF;
    gpio.Pin = PAT_PINMAP_UART7_TX_PIN | PAT_PINMAP_UART7_RX_PIN;
    HAL_GPIO_Init(PAT_PINMAP_UART7_TX_PORT, &gpio);
    HAL_NVIC_SetPriority(UART7_IRQn, 10u, 0u);
    HAL_NVIC_EnableIRQ(UART7_IRQn);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3) {
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8 | GPIO_PIN_9);
    __HAL_RCC_USART3_CLK_DISABLE();
  } else if (huart->Instance == UART5) {
    HAL_GPIO_DeInit(PAT_PINMAP_UART5_TX_PORT, PAT_PINMAP_UART5_TX_PIN);
    HAL_GPIO_DeInit(PAT_PINMAP_UART5_RX_PORT, PAT_PINMAP_UART5_RX_PIN);
    __HAL_RCC_UART5_CLK_DISABLE();
  } else if (huart->Instance == UART7) {
    HAL_GPIO_DeInit(PAT_PINMAP_UART7_TX_PORT, PAT_PINMAP_UART7_TX_PIN | PAT_PINMAP_UART7_RX_PIN);
    __HAL_RCC_UART7_CLK_DISABLE();
  }
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) {
    __HAL_RCC_TIM1_CLK_ENABLE();
  } else if (htim->Instance == TIM3) {
    __HAL_RCC_TIM3_CLK_ENABLE();
  }
}

void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) {
    __HAL_RCC_TIM1_CLK_DISABLE();
  } else if (htim->Instance == TIM3) {
    __HAL_RCC_TIM3_CLK_DISABLE();
  }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim)
{
  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  if (htim->Instance == TIM1) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Alternate = GPIO_AF1_TIM1;
    gpio.Pin = PAT_PINMAP_MEMS_FCLK_Y_PIN;
    HAL_GPIO_Init(PAT_PINMAP_MEMS_FCLK_Y_PORT, &gpio);
  } else if (htim->Instance == TIM3) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Alternate = GPIO_AF2_TIM3;
    gpio.Pin = PAT_PINMAP_MEMS_FCLK_X_PIN;
    HAL_GPIO_Init(PAT_PINMAP_MEMS_FCLK_X_PORT, &gpio);
  }
}
