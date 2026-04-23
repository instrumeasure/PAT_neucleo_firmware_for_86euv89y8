# UM2217 — STM32H7 HAL and LL drivers (reference map)

**Official title:** *Description of STM32H7 HAL and low-layer drivers* (ST user manual **UM2217**). It describes **how Cube HAL and LL are structured and used**, not register bit definitions for the SoC (those stay in the **reference manual** for your part, e.g. RM0433 / RM0468, and in `stm32h7xx_hal_ppp.c` in the Cube pack).

**Typical ST landing (PDF):** [UM2217 on st.com](https://www.st.com/resource/en/user_manual/um2217-description-of-stm32h7-hal-and-lowlayer-drivers-stmicroelectronics.pdf)

Keep the **Cube pack revision** (`STM32_CUBE_H7_FW` / `framework-stm32cubeh7`) aligned with the **UM2217 revision** you care about; API names are stable but details (FIFO, errata notes) drift with pack updates.

## What to read in UM2217 (high level)

| Topic | Use when |
|--------|-----------|
| **General / overview** | Understanding HAL vs LL, handles, multi-instance model. |
| **Dual-core** | H7 dual-core images; not the default single-core Nucleo path in this repo. |
| **Cohabitation HAL + LL** | Mixing LL register access with HAL for the same peripheral — ordering and rules. |
| **Peripheral chapters (SPI, UART, GPIO, …)** | Config structures, polling vs interrupt vs DMA, error and timeout behaviour for that driver. |

## How this repo uses that map

- **Bring-up and tick:** follow **`stm32h7-hal-pitfalls`** (SysTick, `HAL_InitTick`, PLL); UM2217 documents the HAL timebase idea; pitfalls doc is project-specific.
- **Handles, `MspInit`, `State`:** follow **`SKILL.md`** in this folder — that is the distilled pattern (including the **`RESET` → `MspInit`** rule when cloning handles).
- **Silicon truth:** UM2217 + HAL source still lose to **RM** + **errata** for “why does this bit do X?”.

## STMicroelectronics HAL driver source (GitHub component)

The same HAL family ships as the **`stm32h7xx_hal_*`** tree inside **STM32CubeH7**; ST also publishes it as the [stm32h7xx-hal-driver](https://github.com/STMicroelectronics/stm32h7xx-hal-driver) MCU component. Treat GitHub and pack as **the same family**, not necessarily the same **commit** — always match **CMSIS device + HAL** versions per ST release notes.
