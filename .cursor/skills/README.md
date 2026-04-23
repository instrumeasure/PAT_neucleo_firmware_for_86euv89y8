# PAT Nucleo — Cursor agent skills (index)

**Authoritative hardware, protocols, and CSV formats:** [`AGENTS.md`](../../AGENTS.md) at repo root. Read it before changing pins, ADS127 behaviour, or build assumptions.

## Pack layout

See **[`Folder_Structure.md`](Folder_Structure.md)** for the directory tree. Each **`*/SKILL.md`** declares **`metadata.pattern`** (`pipeline` or `tool-wrapper`) and **`metadata.version`** for structured discovery.

## Recommended reading order

1. **`AGENTS.md`** — HAT revision (86euv89y8 J1), SPI order, `STM32_CUBE_H7_FW`, heartbeat fields, app states.
2. **`stm32cube-cmake-pat`** — default **CMake + Ninja** build and **OpenOCD** flash (`scripts/*.ps1`).
3. **`single-channel-spi4-ads127`** — when editing **one ADS127L11** on **SPI4** (`main.c` / `pat_nucleo_h753`) **or** **SPI1–SPI4** (`main_single_ads127_spi.c` / `pat_nucleo_spiN_ads127`), DRDY poll, `ads127_read_sample24_*`, bring-up / post-START gate.
4. **`spi2-pc2c-miso-h7-pat`** — **SPI2** / **PC2_C** MISO, **`PC2SO`**, **`pat_nucleo_spi2_ads127`**, logical ch1, **`arm_skip`** / **`raw24`** zeros, **`MasterKeepIOState`**.
5. **`four-channel-spi-ads127-quartet`** — **SPI1–4**, four ADS127L11, **`ads127_read_quartet_blocking`**, **`pat_nucleo_quartet`**, scan order SPI1→SPI4.
6. **`stm32h7-hal-pitfalls`** — **SysTick** → `HAL_IncTick`, **PLL** → `HAL_InitTick` (hangs, wrong delays).
7. **`stm32cube-hal-model`** — **handles**, **`MspInit` only when `State == RESET`**, `HAL_TIMEOUT`, **UM2217** map.
8. **`platformio-stm32-pat`** — only when the user wants **PIO** / `platformio.ini` / `device monitor`.

## Skill matrix

| Directory | When to apply | Primary repo touchpoints |
|-----------|----------------|---------------------------|
| **`stm32cube-cmake-pat/`** | CMake, Ninja, OpenOCD, missing HAL pack, clean rebuild, flash ELF | `CMakeLists.txt`, `scripts/`, OpenOCD |
| **`single-channel-spi4-ads127/`** | One ADS127 per bus: **SPI4** default app or **SPI1–4** single-bus ELFs, DRDY on MISO, `ads127_*`, settle / post-START gate | `src/main.c`, `src/main_single_ads127_spi.c`, `src/ads127l11.c`, `src/stm32h7xx_hal_msp.c`, `include/pat_pinmap.h` |
| **`spi2-pc2c-miso-h7-pat/`** | SPI2 MISO **PC2/PC2_C**, `PC2SO`, `pat_nucleo_spi2_ads127`, DRDY/`arm_skip`, `MasterKeepIOState` | `src/ads127l11.c`, `src/main_single_ads127_spi.c`, `main_quartet.c`, `main_spi1_4_scan.c`, `pat_pinmap.h` |
| **`four-channel-spi-ads127-quartet/`** | Four ADS127L11 on SPI1–4, epoch order, `ads127_read_quartet_blocking`, `pat_nucleo_quartet` | `src/main_quartet.c`, `src/ads127l11.c`, `CMakeLists.txt`, `examples/four-channel-spi1-4-ads127/README.md` |
| **`stm32h7-hal-pitfalls/`** | `HAL_Delay` stuck, tick not advancing, after `SystemClock_Config` edits | `src/stm32h7xx_it.c`, `SystemClock_Config` in `src/main.c` |
| **`stm32cube-hal-model/`** | `HAL_*_Init`, `HAL_*_MspInit`, cloning `hspi`/`huart`, IRQ guards, UM2217 / LL | `src/main.c`, `src/ads127l11.c` |
| **`platformio-stm32-pat/`** | `pio`, upload via PIO, monitor, populate `framework-stm32cubeh7` for CMake | `platformio.ini` |

## Project rules (globs)

| Path | Role |
|------|------|
| [`.cursor/rules/stm32-firmware.mdc`](../rules/stm32-firmware.mdc) | STM32 C / HAL layout conventions |
| [`.cursor/rules/ads127l11-adc.mdc`](../rules/ads127l11-adc.mdc) | ADS127L11, J1 legacy HAT, SPI notes |

## Authoring new skills

- YAML `name` + `description` (third person, **what** + **when**, trigger keywords).
- Keep **`SKILL.md`** concise (under ~500 lines); use sibling **`reference-*.md`** for long tables.
- Forward slashes in documented paths.

## `AGENTS.md` cross-reference

The `rules_skills` toon in **`AGENTS.md`** lists skills for discovery; this **README** is the full index and reading order.
