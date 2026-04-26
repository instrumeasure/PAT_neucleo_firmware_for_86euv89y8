#include "pat_quartet_spi_irq.h"
#include "stm32h7xx_hal.h"

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
extern SPI_HandleTypeDef hspi4;

#ifndef PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER
#define PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER 0
#endif

#ifndef PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY
#define PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY 6u
#endif

void pat_quartet_spi_parallel_irq_init(void)
{
#if PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER
  (void)hspi1;
  (void)hspi2;
  (void)hspi3;
  (void)hspi4;
#else
  const uint32_t p = (uint32_t)PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY;
  HAL_NVIC_SetPriority(SPI1_IRQn, p, 0u);
  HAL_NVIC_EnableIRQ(SPI1_IRQn);
  HAL_NVIC_SetPriority(SPI2_IRQn, p, 0u);
  HAL_NVIC_EnableIRQ(SPI2_IRQn);
  HAL_NVIC_SetPriority(SPI3_IRQn, p, 0u);
  HAL_NVIC_EnableIRQ(SPI3_IRQn);
  HAL_NVIC_SetPriority(SPI4_IRQn, p, 0u);
  HAL_NVIC_EnableIRQ(SPI4_IRQn);
#endif
}

#if PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER
void SPI1_IRQHandler(void) {}
void SPI2_IRQHandler(void) {}
void SPI3_IRQHandler(void) {}
void SPI4_IRQHandler(void) {}
#else
void SPI1_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi1);
}

void SPI2_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi2);
}

void SPI3_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi3);
}

void SPI4_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi4);
}
#endif
