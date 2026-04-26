#ifndef PAT_QUARTET_SPI_IRQ_H
#define PAT_QUARTET_SPI_IRQ_H

/**
 * SPI1..4 NVIC + IRQ when `pat_nucleo_quartet` uses `HAL_SPI_TransmitReceive_IT`.
 * When **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER`** is ON, init is a no-op and IRQ handlers are
 * empty stubs (parallel sample uses `pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`).
 */
void pat_quartet_spi_parallel_irq_init(void);

#endif
