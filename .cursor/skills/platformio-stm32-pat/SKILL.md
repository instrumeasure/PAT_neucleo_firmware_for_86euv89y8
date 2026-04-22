---
name: platformio-stm32-pat
description: >-
  Optional PlatformIO for PAT Nucleo: py -m platformio run, upload, device
  monitor (ststm32, nucleo_h753zi, stm32cube, 115200 VCP). Primary compile in
  this repo is CMake + STM32Cube HAL — see stm32cube-cmake-pat. Triggers: pio,
  PlatformIO, platformio.ini, upload via PIO, serial monitor, populate
  framework-stm32cubeh7 for CMake.
---

# PlatformIO — PAT Nucleo (STM32 + ADS127L11)

**Optional.** Default build is **`CMakeLists.txt` + STM32CubeH7 pack** — **`.cursor/skills/stm32cube-cmake-pat/SKILL.md`**.

Use PIO when the user wants **`pio` / `py -m platformio`**, CI that already uses PIO, quick **`device monitor`**, or a one-shot run to download the HAL pack under **`.platformio/packages/framework-stm32cubeh7`** for **`STM32_CUBE_H7_FW`**.

## Before acting

1. Read **`platformio.ini`** — `[env:*]`, `board`, `framework`, `monitor_speed`, `upload_protocol`.
2. Align pins and clocks with **`AGENTS.md`** and **`cube/*.ioc`**, not random Nucleo tutorials.

## CLI (directory containing `platformio.ini`)

**PowerShell:** use `;` to chain commands, not `&&`, on older shells.

If **`pio`** is missing, use **`py -m platformio`** (or `python -m platformio`) with the same subcommands.

| Task | Command |
|------|---------|
| Build default env | `py -m platformio run` |
| Build named env | `py -m platformio run -e nucleo_h753zi` |
| Upload | `py -m platformio run -t upload` |
| Serial monitor | `py -m platformio device monitor` |
| List devices | `py -m platformio device list` |
| Clean | `py -m platformio run -t clean` |

**Baud** for ST-Link VCP follows **`monitor_speed`** in **`platformio.ini`** (this project: **115200**).

## Conventions

- **Board:** must match hardware (here: **STM32H753ZI** / **`nucleo_h753zi`** on `ststm32`).
- **Framework `stm32cube`:** HAL layout; sources in this repo under **`src/`**, **`include/`**, **`lib/`**.
- **`build_flags` / `lib_deps`:** match HAL modules and vendored libs; pin versions when stability matters.

## Upload / debug failures

Check ST-Link USB, drivers, and that **no other process** holds the virtual COM port or debugger. Prefer **CMake + `Flash-Stm32CubeOpenOCD.ps1`** if the user asked to avoid PIO.

## HAL tick / SysTick

**`.cursor/skills/stm32h7-hal-pitfalls/SKILL.md`** — applies to PIO `stm32cube` the same as to the CMake HAL build.

## ADS127L11

Protocol and schematic truth: **`AGENTS.md`**, **`.cursor/rules/ads127l11-adc.mdc`**. PIO only changes build/flash/monitor mechanics.

## When `platformio.ini` is absent

Do not invent **`board`**; confirm MCU/Nucleo model before scaffolding `pio project init`.
