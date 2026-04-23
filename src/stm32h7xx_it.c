#include "stm32h7xx_hal.h"

void SysTick_Handler(void)
{
  HAL_IncTick();
}

void HardFault_Handler(void)
{
  /* Halt debugger if attached; otherwise spins (see stm32h7-hal-pitfalls). */
#if defined(__ARMCC_VERSION)
  __breakpoint(0);
#elif defined(__GNUC__)
  __asm volatile("bkpt #0");
#endif
  while (1) {
  }
}
