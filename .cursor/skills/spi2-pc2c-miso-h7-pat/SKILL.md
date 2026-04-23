---
name: spi2-pc2c-miso-h7-pat
description: >-
  PAT NUCLEO-H753ZI + ADS127L11 on SPI2 (logical ch1): PC2 / PC2_C MISO, SYSCFG
  PMCR.PC2SO analog switch, DRDY poll with SPE=0, MasterKeepIOState, init order
  ads127_pins_init before HAL_SPI_Init, pat_nucleo_spi2_ads127. Triggers: SPI2
  MISO PC2, PC2_C, PC2SO, SYSCFG_PMCR, raw24 0x000000, arm_skip, DRDY timeout,
  SPI2 not working, H753 dual pad, PAT_SPI2_PC2SO_OPEN_INSTEAD.
metadata:
  pattern: tool-wrapper
  version: "1.0"
---

# SPI2 + PC2 / PC2_C MISO on STM32H7 (PAT)

## Before acting

1. **[`AGENTS.md`](../../AGENTS.md)** — J1 **86euv89y8**, logical ch1 = SPI2.
2. **[`include/pat_pinmap.h`](../../include/pat_pinmap.h)** — long **PC2_C** / ST Community references (Nucleo vs LQFP100 / VIT6 / load limits).
3. **[`stm32cube-cmake-pat`](../stm32cube-cmake-pat/SKILL.md)** — build + **`Flash-Stm32CubeOpenOCD.ps1 -SingleSpi 2`**.

## Why this pin is special

- CubeMX often labels the ball **PC2_C**; HAL still uses **`GPIOC` `GPIO_PIN_2`** — same physical pin.
- **SYSCFG `PMCR.PC2SO`:** ST FAQ — **0 = analog switch closed**, **1 = open** (pads separated). Wrong default for your package **disconnects digital SPI2 MISO** from the ball → **MISO reads low**, **false DRDY “ready”**, **`arm_skip=1`**, **SCLK runs with no real wait**, **`raw24=0x000000`** or garbage.
- Firmware **`ads127_pins_init()`** sets **`PC2SO` closed by default** (`CLEAR_BIT`). Compile with **`-DPAT_SPI2_PC2SO_OPEN_INSTEAD`** only if bench scope + RM show you need **open** (legacy behaviour).

## Init order (mandatory for SPI2)

Call **`ads127_pins_init()`** before **`HAL_SPI_Init()`** / **`MX_SPI_*`** so **SYSCFG + RCC** for the switch run **before** **`HAL_SPI_MspInit`** muxes **PC2** to **AF5 SPI2**.

Applies to **`main_single_ads127_spi.c`**, **`main_quartet.c`**, **`main_spi1_4_scan.c`** (already ordered); **`main.c`** is SPI4-only.

## SPI init: Master keep I/O state (SPI2 only)

For **`pat_nucleo_spi{1..4}_ads127`**, **`pat_nucleo_h753`**, **`pat_nucleo_quartet`**, and **`pat_nucleo_spi1_4_scan`**, **SPI1–SPI4** use **`MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE`** so when **`__HAL_SPI_DISABLE`** runs during **DRDY** polling, **MISO** stays usable for **`IDR`** reads (SPI4 previously used **DISABLE** and could mis-poll / time out).

## Sample read path (`ads127_read_sample24_ch_blocking`)

- **`!CS` low** → **`delay_after_cs_100ns`** → **`__HAL_SPI_DISABLE`** → **MISO reconfigured to GPIO input with pull-up** → poll **`ch_miso_high_raw`** (IDR) until **low** (SDO/DRDY ready) → **restore MISO AF** → **`__HAL_SPI_ENABLE`** → **`HAL_SPI_TransmitReceive`** 3 bytes → **`!CS` high**.
- **All buses SPI1–SPI4** share this path (GPIO poll avoids unreliable **AF** read with **SPE=0** on H7). **SPI2** still needs correct **`PC2SO`** + init order so IDR tracks the pad.

## Diagnostics (UART / scan logs)

| Symptom | Likely cause |
|--------|----------------|
| **`raw24=0x000000`**, **`arm_skip=1`**, **`to=0`** | MISO stuck low → wrong **`PC2SO`** or init order / damaged pad (see ST threads in `pat_pinmap.h`). |
| **`to` rising**, **`st` timeout** | Real DRDY not seen — wiring, **START**, modulator **CLK**, or **!CS** domain. |
| **`arm_skip=0`**, varying **`raw24`** | Healthy **DRDY wait** + **SPI** data (what you want after fixes). |

## Build and flash (SPI2 single-bus ELF)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -SingleSpi 2
```

ELF: **`cmake-build/pat_nucleo_spi2_ads127.elf`**.

## Code touchpoints

| Path | Role |
|------|------|
| [`src/ads127l11.c`](../../src/ads127l11.c) | **`ads127_pins_init`** (`PC2SO`), **`ads127_read_sample24_ch_blocking`** |
| [`src/main_single_ads127_spi.c`](../../src/main_single_ads127_spi.c) | **`PAT_ADS127_SINGLE_SPI_BUS == 2`**, init order, **`MasterKeepIOState`** |
| [`src/main_quartet.c`](../../src/main_quartet.c) / [`src/main_spi1_4_scan.c`](../../src/main_spi1_4_scan.c) | SPI2 template: **`MasterKeepIOState`** |
| [`src/stm32h7xx_hal_msp.c`](../../src/stm32h7xx_hal_msp.c) | **SPI2** AF on **PB10/PB15**, **PC2** MISO |

## Related skills

| Skill | Use when |
|-------|-----------|
| **`single-channel-spi4-ads127`** | Default **SPI4** / **`pat_nucleo_h753`**, **`main.c`** |
| **`four-channel-spi-ads127-quartet`** | **SPI1–4** epoch, **`pat_nucleo_quartet`** |
| **`stm32cube-cmake-pat`** | CMake, OpenOCD, **`STM32_CUBE_H7_FW`** |
| **`stm32cube-hal-model`** | **`HAL_SPI_MspInit`**, handle **`State`** |

## References (external)

- ST FAQ: [Default state of H7 switches Pxy / Pxy_C](https://community.st.com/t5/stm32-mcus/faq-default-state-of-stm32h7-switches-connecting-pxy-c-and-pxy/ta-p/49300)
- [NUCLEO-H753ZI PC2 / SPI2_MISO](https://community.st.com/t5/stm32-mcus-products/subject-stm32h753zi-cannot-control-pc02-as-gpio-or-spi2-miso/td-p/655439)
- [H735 LQFP100 PC2_C / PC3_C speed / switch](https://community.st.com/t5/stm32-mcus-products/stm32h735v-lqfp100-pc2-c-and-pc3-c-speed/td-p/214053)
- [H753VIT6 PC2_C / load / errata ES0392](https://community.st.com/t5/stm32-mcus-products/stm32h753vit6-pc2-c-pin/td-p/739240)
