---
name: stm32cube-cmake-pat
description: >-
  Default PAT compile path: CMake + STM32CubeH7 HAL pack (root CMakeLists.txt,
  Ninja, gcc-arm-none-eabi). STM32_CUBE_H7_FW, scripts/Build-Stm32CubeCMake.ps1,
  Flash-Stm32CubeOpenOCD.ps1 (OpenOCD, ST-Link V3). PAT Nucleo STM32H753ZI
  + Legacy QPD ADC HAT 86euv89y8. Triggers: STM32Cube, CMake compile, Ninja,
  flash ELF, OpenOCD, compile without PlatformIO, cube build, CLT toolchain.
---

# STM32Cube CMake — PAT Nucleo (H753ZI)

## Before acting

1. Read **`AGENTS.md`** — hardware baseline, USART heartbeat format, quartet read order.
2. **`CMakeLists.txt`** (root) is the **authoritative** build; **`platformio.ini`** is optional.

## Requirements

| Tool | Notes |
|------|--------|
| **CMake** ≥ 3.20 | Configure |
| **Ninja** | On `PATH` (e.g. `winget install Ninja-build.Ninja`); build script refreshes machine+user PATH on Windows. |
| **arm-none-eabi-gcc** | STM32CubeCLT / GNU Arm Embedded |
| **STM32CubeH7** tree | Env **`STM32_CUBE_H7_FW`**, or default **`%USERPROFILE%\.platformio\packages\framework-stm32cubeh7`** after `py -m platformio run -e nucleo_h753zi` once, **or** an ST **STM32Cube FW_H7** extraction |

Verify pack: **`Drivers/CMSIS/Include/cmsis_compiler.h`** exists.

## Build (Windows, repo root)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
```

**Outputs:** `cmake-build/pat_nucleo_h753.elf`, `pat_nucleo_h753.bin`, `compile_commands.json`.

**Custom HAL path:** `powershell` then `$env:STM32_CUBE_H7_FW = 'C:\path\to\STM32Cube_FW_H7';` run the script, or set the variable in System / user environment.

## Flash (OpenOCD + ST-Link)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1
```

- Resolves **`openocd.exe`** from `PATH`, else **`%USERPROFILE%\.platformio\packages\tool-openocd\bin\openocd.exe`**.
- Targets **`cmake-build/pat_nucleo_h753.elf`** by default; optional `-Elf` path.
- Uses **`interface/stlink.cfg`** + **`target/stm32h7x.cfg`** and `program "<elf>" verify reset exit` — do **not** hand-edit nested quotes in PowerShell `-c`; keep the script’s `-f`-style command string.

If flash fails: board powered, USB ST-Link connected, close anything holding the ST-Link/VCP exclusively.

## Repo layout

| Path | Role |
|------|------|
| **`cmake/gcc-arm-none-eabi.cmake`** | Toolchain |
| **`cube/linker/STM32H753ZITx_FLASH.ld`** | Linker script |
| **`scripts/Ensure-STM32CubeFrameworkLink.ps1`** | Symlink PIO framework pack when needed |

## Related skills

- **`.cursor/skills/platformio-stm32-pat/SKILL.md`** — optional PIO build/monitor.
- **`.cursor/skills/stm32h7-hal-pitfalls/SKILL.md`** — SysTick / PLL tick (same HAL for CMake and PIO).
