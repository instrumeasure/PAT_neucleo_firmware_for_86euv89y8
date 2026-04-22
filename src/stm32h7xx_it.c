/**
 * CMSIS weak vectors default SysTick_Handler to Default_Handler (infinite loop).
 * HAL_SysTick enable would trap on first tick unless we forward to HAL_IncTick().
 */
#include "stm32h7xx_hal.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void TIM6_DAC_IRQHandler(void)
{
}

void SPI6_IRQHandler(void)
{
}
