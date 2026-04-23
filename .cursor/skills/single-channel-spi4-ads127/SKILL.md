---
name: single-channel-spi4-ads127
description: >-
  PAT NUCLEO-H753ZI + one TI ADS127L11: default SPI4 / logical ch3 (main.c,
  pat_nucleo_h753) or single-bus SPI1–SPI4 (main_single_ads127_spi.c,
  pat_nucleo_spiN_ads127). DRDY on MISO via GPIO pull-up poll, ads127_bringup,
  post_start_gate, ADS127_START_STREAM_SETTLE_MS. Triggers: ADS127L11,
  sample24, DRDY, PAT_LOG_CH, Flash-Stm32CubeOpenOCD -SingleSpi.
metadata:
  pattern: tool-wrapper
  version: "1.0"
---

# Single-channel ADS127L11 (PAT) — SPI4 default + SPI1–4 single-bus

## Package identifier (folder vs scope)

**Directory and `name`:** `single-channel-spi4-ads127` — **historical** (the first single-bus app was SPI4 + `main.c`). **Skill scope today:** one ADS127 on **SPI4** (`main.c`, `pat_nucleo_h753`) **or** **SPI1–SPI4** (`main_single_ads127_spi.c`, `pat_nucleo_spiN_ads127`). Renaming the folder requires updating **`AGENTS.md`** `rules_skills`, **`.cursor/skills/README.md`**, and sibling **`../…/SKILL.md`** links.

## Before acting

1. Read **[`AGENTS.md`](../../AGENTS.md)** — J1 legacy HAT **86euv89y8**, shared START/RESET, SPI ordering for quartet vs single-channel apps.
2. Read **[`.cursor/rules/ads127l11-adc.mdc`](../../.cursor/rules/ads127l11-adc.mdc)** — TI SPI, SDO/DRDY, J1 pin semantics.
3. **SPI4 default example:** **[`examples/single-channel-spi4-ads127/README.md`](../../examples/single-channel-spi4-ads127/README.md)**.
4. **SPI1–4 single-bus example codebase:** **[`examples/single-bus-spi1-4-ads127/README.md`](../../examples/single-bus-spi1-4-ads127/README.md)** (`pat_nucleo_spiN_ads127`, **`Flash-Stm32CubeOpenOCD.ps1 -SingleSpi N`**).
5. **[`stm32cube-cmake-pat`](../stm32cube-cmake-pat/SKILL.md)** — CMake + OpenOCD.

## Hardware

- **MCU:** NUCLEO-H753ZI; **HAT:** **86euv89y8** J1.
- **Default app (`main.c`):** **SPI4 = logical ch3** — `!CS` **PE11**, **SCK** PE12, **MOSI** PE6, **MISO** PE13 (AF5 SPI4). ELF **`pat_nucleo_h753`**.
- **Single-bus apps (`main_single_ads127_spi.c`):** compile-time **`PAT_ADS127_SINGLE_SPI_BUS`** → **`pat_nucleo_spi1_ads127` … `pat_nucleo_spi4_ads127`**; **`PAT_LOG_CH`** for UART labels. Pins: **`include/pat_pinmap.h`** / **`PINMAP.md`** (e.g. SPI3 **PA15** !CS, **PC11** MISO).
- **Shared:** **PF0** nRESET, **PF1** START, modulator **CLK** from HAT.

## Software map

| Path | Role |
|------|------|
| [`src/main.c`](../../src/main.c) | Default: SPI4, `ads127_bringup`, `ads127_post_start_gate`, `ads127_read_sample24_blocking`, ~1 Hz printf |
| [`src/main_single_ads127_spi.c`](../../src/main_single_ads127_spi.c) | One of SPI1–4 via **`PAT_ADS127_SINGLE_SPI_BUS`**, same bring-up / gate / stream pattern |
| [`src/ads127l11.c`](../../src/ads127l11.c) / [`src/ads127l11.h`](../../src/ads127l11.h) | CS/RESET/START, RREG/WREG, **`ads127_read_sample24_ch_blocking`**, **`ADS127_START_STREAM_SETTLE_MS`**, **`ads127_post_start_gate`**, **`ads127_after_failed_post_start_gate`**, shadow / **`ads127_diag_t`** (`drdy_skipped_arm_high` UART **`arm_skip`**) |
| [`src/stm32h7xx_hal_msp.c`](../../src/stm32h7xx_hal_msp.c) | `HAL_SPI_MspInit` per SPI instance |
| [`src/pat_clock.c`](../../src/pat_clock.c) | PLL + `HAL_InitTick` after clock change |
| [`CMakeLists.txt`](../../CMakeLists.txt) | **`pat_nucleo_h753`**, **`pat_nucleo_spi1_ads127` … `pat_nucleo_spi4_ads127`** |

## SPI + 24-bit sample read contract

- **SPI mode 1 (HAL):** `CLKPolarity` low, `CLKPhase` **2nd edge** (`SPI_PHASE_2EDGE`). `NSS` = **software**; `!CS` toggled only in [`ads127l11.c`](../../src/ads127l11.c).
- **`ads127_read_sample24_ch_blocking`:** `!CS` low → **`delay_after_cs_100ns`** → **`__HAL_SPI_DISABLE`** → **MISO → GPIO input, pull-up** → poll **IDR** until **low** (SDO/DRDY ready) → **MISO → AF** → **`__HAL_SPI_ENABLE`** → **`HAL_SPI_TransmitReceive`** 3 bytes → `!CS` high. No “wait for high first” before the low poll (per current policy). **`drdy_skipped_arm_high` / `arm_skip`:** **1** if MISO was **already low** when the GPIO poll started (first **`HAL_SPI_TransmitReceive`** can occasionally **`HAL_TIMEOUT`** — bench intermittency).
- **SPI2 / PC2_C:** **`PC2SO`**, init order, **`MasterKeepIOState`**: **`spi2-pc2c-miso-h7-pat`**.
- **SPI3:** **`ads127_post_start_gate`** RREG with **START** low may still return **-2** / zero shadow intermittently; with **`PAT_ADS127_STRICT_BRINGUP=OFF`**, **`ads127_after_failed_post_start_gate`** helps streaming recover.

## Post-START settle and gate

- After **`ads127_start_set(1)`**, all mains use **`HAL_Delay(ADS127_START_STREAM_SETTLE_MS)`** (default **25 ms** in `ads127l11.h`) before **`ads127_post_start_gate`** / first sample.
- **`ads127_post_start_gate`:** holds **START** low briefly, **`ads127_shadow_refresh`** (RREGs), restores **START**; returns **0** or negative codes (see `ads127l11.h`). Non-strict: on failure, **`ads127_after_failed_post_start_gate()`** then continue streaming.

## Throughput and UART

- [`main.c`](../../src/main.c) and [`main_single_ads127_spi.c`](../../src/main_single_ads127_spi.c) call **`ads127_read_sample24_*` in a tight loop** so throughput follows **ADC ODR** (blocking wait on DRDY). **`printf`** and LED toggle run about **once per second** (`HAL_GetTick`) so the UART path does not cap sampling.

## `f_SCLK` and prescaler

- **Default `main.c`:** bit rate ≈ \(f_{\mathrm{SPI4\_kernel}} / \mathrm{prescaler}\) from **`HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4)`**; keep **`MX_SPI4_Init.BaudRatePrescaler`** and printed divisor in sync.
- **`main_single_ads127_spi.c`:** same idea for the selected **`SPI1`…`SPI4`** kernel clock and prescaler — see that file’s banner print.

## Build and flash

Use **[`stm32cube-cmake-pat`](../stm32cube-cmake-pat/SKILL.md)** — from repo root:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1
```

Default ELF: **`cmake-build/pat_nucleo_h753.elf`** (SPI4 + `main.c`).

**Single-bus SPI N only** (`main_single_ads127_spi.c`):

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -SingleSpi 1
```

Replace **`1`** with **`2`** … **`4`** for **`pat_nucleo_spi2_ads127`** … **`pat_nucleo_spi4_ads127`**.

## Related skills

| Skill | Use when |
|-------|-----------|
| **`stm32cube-cmake-pat`** | CMake, Ninja, OpenOCD, `STM32_CUBE_H7_FW` |
| **`stm32h7-hal-pitfalls`** | SysTick / `HAL_InitTick` after PLL |
| **`stm32cube-hal-model`** | `HAL_SPI_Init`, `HAL_SPI_MspInit`, `State == RESET`, cloning handles |
| **`spi2-pc2c-miso-h7-pat`** | **`pat_nucleo_spi2_ads127`**, **PC2** / **`PC2SO`**, logical ch1 |
| **`four-channel-spi-ads127-quartet`** | **SPI1–4** epoch, **`pat_nucleo_quartet`**, shared START vs single-bus |
| **`platformio-stm32-pat`** | Optional PlatformIO path |

## References

- TI **ADS127L11** / **SBAS946** (SPI, DRDY, registers).
- STM32H753 **RM** + Cube **HAL** SPI master.
