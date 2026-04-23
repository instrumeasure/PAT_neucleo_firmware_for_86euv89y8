# Example: single SPI4 + ADS127L11 (logical channel 3)

This folder documents the **default** PAT firmware application: one TI ADS127L11 on **SPI4** with **software `!CS`**, **SDO/DRDY polled on MISO**, and **USART3** logging. It is the canonical reference for bringing up **one** ADC channel before scaling to SPI1–4.

## Hardware

- **MCU:** NUCLEO-H753ZI  
- **HAT:** legacy **86euv89y8** — **J1** only for this example (not J2 / SPI6).  
- **This channel:** SPI4 = “logical ch3” on the quartet (`!CS` = PE11, SCK/MOSI/MISO = PE12/PE6/PE13).  
- **Shared:** `PF0` nRESET, `PF1` START (all four ADS127 devices). Modulator **CLK** comes from the HAT — MCU does not drive it in this build.

Authoritative pin table: **`PINMAP.md`** (J1 SPI4 row) and **`include/pat_pinmap.h`**.

## Firmware layout

| File | Role |
|------|------|
| `src/main.c` | `main()`: clock, USART3, `MX_SPI4_Init()`, probe pulse, `ads127_bringup`, START, periodic `ads127_read_sample24_blocking` + printf |
| `src/ads127l11.c` | CS/RESET/START GPIO, `ads127_rreg` / `ads127_wreg`, `ads127_read_sample24_blocking` (DRDY poll + 24-bit read) |
| `src/ads127l11.h` | Register/cmd macros, `ads127_shadow_t`, `ads127_diag_t` |
| `src/stm32h7xx_hal_msp.c` | `HAL_SPI_MspInit` — SPI4 GPIO AF on PE |
| `src/pat_clock.c` | `PAT_SystemClock_Config()` — PLL + `HAL_InitTick` after clock change |
| `src/stm32h7xx_it.c` | SysTick / fault vectors used by HAL |
| `CMakeLists.txt` | Default `APP_SRC` lists `main.c` + above → **`pat_nucleo_h753.elf`** |

Other tree roots (`main_spi6.c`, `main_spi_test.c`) are **separate** targets; they are not this example.

## SPI and sample read behaviour

- **Mode:** SPI mode **1** in HAL terms: `CLKPolarity` low, `CLKPhase` **2nd edge** (`SPI_PHASE_2EDGE`).  
- **`!CS`:** GPIO **manual** (`SPI_NSS_SOFT`); all ADS127 transfers assert CS in driver code, not from the SPI peripheral NSS.  
- **`f_SCLK`:** set in `MX_SPI4_Init()` via `BaudRatePrescaler`; nominal bit rate is \(f_{\mathrm{SPI4\_ker}} / \mathrm{prescaler}\) (see `HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI4)` in `main.c`).  
- **24-bit conversion read** (`ads127_read_sample24_blocking`): assert `!CS` → short post-CS delay → poll **MISO** until **low** (DRDY / SDO mode) → `HAL_SPI_TransmitReceive` **3×** 0x00 → deassert `!CS` immediately after HAL returns (no extra busy-wait there). **No** “arm MISO high” phase in the current code.

`main.c` calls `ads127_read_sample24_blocking` **continuously** in the main loop (throughput set by DRDY / ODR); **printf** (and LED toggle) run about **once per second** so the UART path does not limit sampling.

## Build and flash

From repo root (see **`AGENTS.md`** and **`.cursor/skills/stm32cube-cmake-pat/SKILL.md`**):

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1
```

Output: **`cmake-build/pat_nucleo_h753.elf`**.

## References

- TI **ADS127L11** / **SBAS946** — SPI, DRDY/SDO, timing, registers.  
- **`.cursor/rules/ads127l11-adc.mdc`** — PAT-specific J1 notes.  
- STM32H753 **RM** + **HAL** — SPI master, GPIO AF, RCC kernel clock for SPI4.
