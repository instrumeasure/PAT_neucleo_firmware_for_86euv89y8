/**
 * CMSIS weak vectors default SysTick_Handler to Default_Handler (infinite loop).
 * HAL_SysTick enable would trap on first tick unless we forward to HAL_IncTick().
 */
#include "stm32h7xx_hal.h"

extern TIM_HandleTypeDef htim6;
extern SPI_HandleTypeDef hspi6;

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

void SPI6_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi6);
}
