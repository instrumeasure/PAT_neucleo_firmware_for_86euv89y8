---
name: stm32cube-hal-model
description: >-
  STM32Cube HAL driver model for this firmware (aligned with ST UM2217): PPP_HandleTypeDef,
  State/Lock, when HAL_PPP_MspInit runs (typically only if State == RESET), HAL_Init tick,
  HAL vs LL, and debugging HAL_TIMEOUT. Use when editing HAL_Init/MspInit, cloning SPI/UART
  handles, reading UM2217 HAL/LL chapters, or explaining HAL behaviour vs RM / stm32h7xx_hal_ppp.c
  in the Cube pack.
---

# STM32Cube HAL — driver model (PAT firmware)

HAL is normal C on top of CMSIS: it programs registers and tracks **per-handle** state. The **reference implementation** is `stm32h7xx_hal_ppp.c` under `STM32_CUBE_H7_FW` / `framework-stm32cubeh7` — read it when behaviour is unclear.

| Symptom | Read first |
|---------|------------|
| Hang / **`HAL_Delay`** never returns / tick frozen | **`stm32h7-hal-pitfalls`** |
| **`HAL_TIMEOUT`**, no SCLK after copying **`hspi`**, MSP not running | This skill |
| CMake / Ninja / flash | **`stm32cube-cmake-pat`** |

## ST user manual UM2217

ST publishes the **H7 HAL and LL driver description** as **[UM2217](https://www.st.com/resource/en/user_manual/um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf)** (*Description of STM32H7 HAL and low-layer drivers*). Use it for **API structure**, **per-driver chapters** (SPI, UART, GPIO, DMA, …), **HAL vs LL**, and **dual-core / cohabitation** rules. Use the **reference manual (RM)** for register-level truth and the **Cube HAL sources** for exact sequencing on your pack revision.

For a short chapter map and links, read [reference-um2217.md](reference-um2217.md).

## Handle

`SPI_HandleTypeDef`, `UART_HandleTypeDef`, etc. hold:

- **`Instance`** — which peripheral (`SPI4`, `USART3`, …).
- **`Init`** — user-facing configuration (mode, prescaler, phase, …).
- **Internal:** `State`, `Lock`, buffer pointers, errors; optional `hdma_*`, callbacks.

## `State` and `Lock`

- **`Lock`**: short critical sections (`HAL_LOCK` / `HAL_UNLOCK`); avoid using the same handle from ISR and foreground without the IRQ-safe APIs.
- **`State`**: coarse lifecycle, e.g. `RESET` → `READY` → `BUSY` → `READY` / `ERROR`. Many APIs require **`READY`** before starting a transfer.

## `HAL_PPP_Init` vs `HAL_PPP_MspInit`

- **`HAL_PPP_Init`**: parameter checks, often disables peripheral, writes its registers from `Init`, sets `State` to `READY` (or `ERROR`).
- **`HAL_PPP_MspInit`**: **board wiring** — RCC enable for that `Instance`, GPIO AF, NVIC, DMA link. Usually implemented in `main.c` / `main_spi_test.c` (or `*_msp.c` in full Cube exports).

**Critical pattern (many drivers):** `HAL_PPP_MspInit` is invoked **only when `hspi->State == HAL_SPI_STATE_RESET`** on entry to `HAL_SPI_Init` (same idea varies slightly by driver; always verify in `hal_ppp.c`).

If you **`memcpy` or struct-assign** one inited handle to another (`hspi4 = hspi1`) and only change **`Instance`**, the copy often brings **`State == READY`**. The next **`HAL_SPI_Init(&hspi4)`** then **skips `HAL_SPI_MspInit`** → no RCC/GPIO for SPI4 (symptom: **no SCLK on the pin**, **`HAL_TIMEOUT`** on transfers). **Fix:** set `hspiN.State = HAL_SPI_STATE_RESET` before `HAL_SPI_Init(&hspiN)`, or initialise each handle from zero / duplicate the full `Init` block as CubeMX does.

## `HAL_Init` and timebase

`HAL_Init()` configures SysTick for **`HAL_GetTick` / `HAL_Delay`**. After changing PLL in `SystemClock_Config`, call **`SystemCoreClockUpdate()`** and **`HAL_InitTick(...)`**. See **`stm32h7-hal-pitfalls`** for `SysTick_Handler` → `HAL_IncTick`.

## Debugging checklist

1. Did **`MspInit`** run for this **`Instance`**? (clocks, pins, NVIC/DMA.)
2. Is **`State`** appropriate for the API (`READY` before `Transmit` / `TransmitReceive`)?
3. Does **`Init`** match the datasheet / slave (CPOL/CPHA, NSS, FIFO on H7 SPI)?
4. **`HAL_TIMEOUT`**: peripheral or slave not finishing — often wiring, CS, or kernel clock off — not “random HAL bug” until `hal_ppp.c` proves otherwise.

## HAL vs LL vs registers

- **HAL**: portable, higher level; respect handle state and MSP rules.
- **LL**: thinner, closer to registers.
- **RM0433 + `hal_ppp.c`**: authoritative for silicon + actual driver order.

## IRQ dispatch vs `Instance`

For **`TIM6_DAC_IRQHandler` / `SPI6_IRQHandler`**, guard with **`htim6.Instance == TIM6`** (and **`hspi6.Instance == SPI6`**) before calling **`HAL_*_IRQHandler`**. The address of a handle is never `NULL` in C; **`Instance == NULL`** means the handle was never **`HAL_Init`**’d (e.g. SPI-test image stubs).

## Related in this repo

- **`.cursor/skills/README.md`** — skill index and reading order.
- **`stm32h7-hal-pitfalls`** — SysTick, PLL tick, `stm32h7xx_it.c`.
- **`stm32cube-cmake-pat`** / **`platformio-stm32-pat`** — where the Cube HAL pack lives on disk.
- **`PINMAP.md`** — project pins; HAL policy follows ST UM2217 / Cube pack sources.
- **[reference-um2217.md](reference-um2217.md)** — UM2217 scope, when to open which ST doc.

Project pinout (`cube/*.ioc`) and schematics override generic HAL tutorials.
