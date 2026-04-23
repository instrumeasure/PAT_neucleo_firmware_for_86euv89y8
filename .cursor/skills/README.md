# PAT Nucleo — Cursor agent skills (index)

**Authoritative hardware, protocols, and CSV formats:** [`AGENTS.md`](../../AGENTS.md) at repo root. Read it before changing pins, ADS127 behaviour, or build assumptions.

## Recommended reading order

1. **`AGENTS.md`** — HAT revision (86euv89y8 J1), SPI order, `STM32_CUBE_H7_FW`, heartbeat fields, app states.
2. **`stm32cube-cmake-pat`** — default **CMake + Ninja** build and **OpenOCD** flash (`scripts/*.ps1`).
3. **`stm32h7-hal-pitfalls`** — **SysTick** → `HAL_IncTick`, **PLL** → `HAL_InitTick` (hangs, wrong delays).
4. **`stm32cube-hal-model`** — **handles**, **`MspInit` only when `State == RESET`**, `HAL_TIMEOUT`, **UM2217** map.
5. **`platformio-stm32-pat`** — only when the user wants **PIO** / `platformio.ini` / `device monitor`.

## Skill matrix

| Directory | When to apply | Primary repo touchpoints |
|-----------|----------------|---------------------------|
| **`stm32cube-cmake-pat/`** | CMake, Ninja, OpenOCD, missing HAL pack, clean rebuild, flash ELF | `CMakeLists.txt`, `scripts/`, OpenOCD |
| **`stm32h7-hal-pitfalls/`** | `HAL_Delay` stuck, tick not advancing, after `SystemClock_Config` edits | `src/stm32h7xx_it.c`, `SystemClock_Config` in `src/main.c` |
| **`stm32cube-hal-model/`** | `HAL_*_Init`, `HAL_*_MspInit`, cloning `hspi`/`huart`, IRQ guards, UM2217 / LL | `src/main.c`, `src/ads127l11_hal_stm32.c` |
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
