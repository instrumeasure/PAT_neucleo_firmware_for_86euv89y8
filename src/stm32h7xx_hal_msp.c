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
  }
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3) {
    return;
  }

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF7_USART3;
  gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
  HAL_GPIO_Init(GPIOD, &gpio);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3) {
    return;
  }
  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8 | GPIO_PIN_9);
  __HAL_RCC_USART3_CLK_DISABLE();
}
