---
name: stm32h7-hal-pitfalls
description: >-
  STM32H7 cortex-M7 + STM32Cube HAL in this repo: SysTick_Handler and
  HAL_IncTick, HAL_InitTick after PLL and SystemCoreClockUpdate, stm32h7xx_it.c
  compiled. Applies to CMake+HAL and PlatformIO stm32cube builds. Symptoms:
  hung CPU, LED stuck on, HAL_Delay infinite or wrong timing, weak vector to
  Default_Handler. NUCLEO-H753ZI, USART3 VCP.
metadata:
  pattern: tool-wrapper
  version: "1.0"
---

# STM32H7 + STM32Cube HAL — bring-up pitfalls (this repo)

Same HAL rules whether you build with **CMake + STM32Cube pack** (`stm32cube-cmake-pat`) or **PlatformIO + stm32cube** (`platformio-stm32-pat`). CMSIS startup does not magically wire SysTick for you.

## 1. `SysTick_Handler` is not optional

GCC startup maps **`SysTick_Handler`** weak → **`Default_Handler`** (infinite loop). **`HAL_Init()`** enables SysTick for the 1 ms HAL tick.

Without a proper **`SysTick_Handler`** calling **`HAL_IncTick()`**:

- **`HAL_GetTick()` never advances** → **`HAL_Delay()` can block forever**.
- Or the **first SysTick IRQ** hits **`Default_Handler`** and never returns.

**Symptoms:** GPIO appears stuck before the first **`HAL_Delay`** (e.g. user LED solid), or apparent hang.

**Fix:** **`src/stm32h7xx_it.c`** must define:

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
}
```

Ensure that file is **in the CMake/app sources** (this repo includes it).

## 2. Reload SysTick after changing the PLL

After **`HAL_RCC_ClockConfig()`**, frequency changed from what **`HAL_Init()`** assumed.

**After successful clock configuration:**

- **`SystemCoreClockUpdate()`**
- **`HAL_InitTick(TICK_INT_PRIORITY)`**

Without this, wall-clock delays are wrong.

## 3. STM32H7 init order (high level)

Follow your **`SystemClock_Config()`**: PWR/voltage scaling if applicable, oscillator/PLL, **`FLASH_LATENCY`** for target SYSCLK. Do not guess SMPS vs LDO without matching the board.

## 4. “Framework” does not ship a full interrupt table

Unlike a full CubeIDE export, you may need to **add** **`stm32h7xx_it.c`** (or equivalent). SysTick is mandatory for the HAL tick; HardFault/NMI handling is optional.

## 5. NUCLEO-H753ZI sanity (this firmware)

- User LEDs: commonly **PB0 / PB7 / PB14**.
- **USART3** **PD8/PD9** → ST-Link VCP; baud in **`platformio.ini`** **`monitor_speed`** where PIO is used; CMake build still uses same UART pins in code.

## 6. Duplicate `main`

Only one **`main`**. Alternate harnesses (e.g. LED-only) live outside default **`src/`** (this repo: **`extras/minimal_led_main.c`**) so they are not linked by mistake.

## Related

- **`AGENTS.md`** — heartbeat CSV, ADS127 quartet order.
- **`.cursor/rules/stm32-firmware.mdc`**
- **`stm32cube-cmake-pat`** / **`platformio-stm32-pat`** — how you **build**, not analogue/SPI facts.

Project code and schematics win over generic tutorials.
