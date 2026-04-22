#ifndef ADS127L11_HAL_STM32_H
#define ADS127L11_HAL_STM32_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"

#define ADS127_CHANNEL_COUNT 4U

void ADS127_HAL_BindSpi(uint8_t channel, SPI_HandleTypeDef *hspi);
void ADS127_HAL_SetCsPin(uint8_t channel, GPIO_TypeDef *port, uint16_t pin);
void ADS127_HAL_SetStartPin(GPIO_TypeDef *port, uint16_t pin);
void ADS127_HAL_SetResetPin(GPIO_TypeDef *port, uint16_t pin);

HAL_StatusTypeDef ADS127_HAL_SPI_Transfer(uint8_t channel, const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_ms);
void ADS127_HAL_SetCS(uint8_t channel, bool asserted);
void ADS127_HAL_SetSTART(bool high);
void ADS127_HAL_SetRESET(bool high);
void ADS127_HAL_ToggleRESET(uint32_t pulse_ms);
void ADS127_HAL_DelayMs(uint32_t ms);

#endif
