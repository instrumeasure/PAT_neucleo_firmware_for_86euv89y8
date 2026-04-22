# PAT Nucleo Firmware (Legacy 86euv89y8)

STM32H753ZI + four ADS127L11 (QPD HAT **J1**), QPD DSP (3×16 ring), **SPI6** **slave** to inter-HAT **J2** (`fmt` 0x02, 64 B).

## Build (primary: CMake + STM32Cube HAL)

```powershell
$fw = "$env:USERPROFILE\.platformio\packages\framework-stm32cubeh7"
$tc = (Resolve-Path "cmake\gcc-arm-none-eabi.cmake").Path
cmake -B cmake-build -G Ninja -DCMAKE_TOOLCHAIN_FILE="$tc" -DSTM32_CUBE_H7_FW=$fw
cmake --build cmake-build
```

Output: `cmake-build/pat_nucleo_h753.elf` and `.bin`. See [AGENTS.md](AGENTS.md) for `STM32_CUBE_H7_FW` and `scripts/Build-Stm32CubeCMake.ps1`.

**Optional — PlatformIO:** `py -m platformio run` (same sources under `src/`).

## Serial monitor (ST-Link VCP USART3)

```powershell
py -m platformio device monitor
```

`115200` baud (`platformio.ini` / `main.c`).

## Heartbeat telemetry

One line about every 1 s on USART3 (matches `printf` in `heartbeat_tick()`):

`HB,<state>,<tick_ms>,<quartets_ok>,0x______,0x______,0x______,0x______`

- `quartets_ok` = `ads127_get_quartet_acquired_count()`.
- Raw fields = last good 24-bit words per channel, or `0xFFFFFF` invalid.

## SPI6 J2 (PolarFire or bench master)

- **Pins:** PA5 SCK (in), PG8 MISO, PG12 MOSI, PG14 NSS (AF8).
- **Frame:** 64 B, `sample_index` uint32 LE, `fmt` 0x02, `p_lo`, then 4×3× 24 b BE **DSP** (raw / I / Q per ch), 20 B pad. Master clocks **64** B per CS (plan §6.3.1 (A)).

## ADS127 clock and gate

- Default `ADS127_CONFIG4_USER=0x80` (external **25** MHz on CLK) in [lib/ADS127TI/include/ads127l11.h](lib/ADS127TI/include/ads127l11.h). Override with `-DADS127_CONFIG4_USER=0` at compile time if using internal osc only.
- `SAMPLE_RATE_HZ` in `main.c` tracks **~48.8 kHz** when external 25 MHz path is selected.

## Source layout

- `src/main.c` — FSM, TIM6 gate, quartet + DSP + SPI6 pack, SPI1–4/SPI6/USART3.
- `src/qpd_dsp.c` / `include/qpd_dsp.h` — three paths, 3×16 rings, `(sum)>>4`.
- `src/qpd_spi6_slave.c` / `include/qpd_spi6_slave.h` — 64 B TX shadow, `HAL_SPI` IT, callbacks.
- `src/stm32h7xx_it.c` — `SysTick_Handler`, `TIM6_DAC_IRQHandler`, `SPI6_IRQHandler`.
- `lib/ADS127TI/src/ads127l11.c` — ADS127 bring-up and quartet read.
- `cube/legacy_86euv89y8_h753zi.ioc` — Cube pin snapshot.

Onboarding and review packets: **[AGENTS.md](AGENTS.md)**.
