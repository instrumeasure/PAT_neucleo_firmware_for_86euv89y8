/**
 * @file pat_quartet_p4_dma.h
 * Tier B/C (plan): overlapping SPI1–4 24-bit reads via DMA — **not implemented** in current firmware.
 *
 * Gates before enabling quartet SPI DMA on STM32H7 (see plan “STM32 expert review”):
 *
 * 1. **D-cache:** Place RX buffers in **non-cacheable** MPU regions or **DTCM**, or perform correct
 *    **SCB_CleanDCache_by_Addr / InvalidateDCache_by_Addr** around DMA (RM0433 + HAL examples).
 * 2. **One in-flight transfer per `hspi`:** Do not start a second `HAL_SPI_TransmitReceive_DMA` on the
 *    same handle until the first completes (HAL state machine).
 * 3. **NVIC priorities:** SPI / DMA stream IRQs vs **SysTick** (`HAL_IncTick`) — avoid starving the 1 ms tick.
 * 4. **No `printf` from DMA or SPI completion ISRs** — signal main (flag) for throttled logging.
 * 5. **DRDY GPIO phase:** `ads127_read_sample24_ch_blocking` reconfigures MISO for GPIO poll then AF SPI;
 *    reconcile with DMA (e.g. EXTI DRDY arm per channel, or DMA only for the 3-byte phase after ready).
 *
 * `pat_quartet_epoch_line_t` is **32-byte aligned** in `pat_quartet_epoch.h` to ease a future DMA + cache policy.
 */

#ifndef PAT_QUARTET_P4_DMA_H
#define PAT_QUARTET_P4_DMA_H

#define PAT_QUARTET_DMA_RX_ALIGN_BYTES 32u

#endif
