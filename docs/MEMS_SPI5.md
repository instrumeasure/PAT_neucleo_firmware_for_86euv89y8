# SPI5 MEMS bring-up (pat_nucleo_mems_bringup)

This note tracks the implementation footprint for the SPI5 AD5664R + MEMS control path and the UART5/UART7 command bridge.

## Target and sources

- CMake target: `pat_nucleo_mems_bringup`
- Entry point: `src/main_mems_bringup.c`
- Core modules:
  - `src/ad5664r.c`
  - `src/pat_mems_regs.c`
  - `src/pat_mems_sm.c`
  - `src/pat_uart5_pat5.c`
  - `src/pat_uart7_laser.c`

## Pin routing (single source: `include/pat_pinmap.h`)

- SPI5: `PF6` (!CS GPIO), `PF7` (SCK AF5), `PF9` (MOSI AF5)
- MEMS clocks:
  - `PC9` = `TIM3_CH4` (AF2, FCLK_X)
  - `PA8` = `TIM1_CH1` (AF1, FCLK_Y)
- MEMS enable: `PA9` (GPIO)
- UART5 PolarFire: `PC12` TX / `PD2` RX (AF8)
- UART7 laser: `PE8` TX / `PE7` RX (AF7)
- Laser GPIO: `PB8` = `laser_driver_oc` input, `PB9` = `int_lock` output

## Runtime model

- `pat_mems_reg_block_t` is a 32-byte in-RAM register mirror with `commit_seq` coherency guard.
- `pat_mems_sm` handles staged transitions:
  - `OFF` -> `DAC_INIT` -> `FCLK_RUN` -> `ARMED` -> `EN_ON`
- SPI5 pump is periodic from `main` (`PAT_MEMS_DAC_PUMP_PERIOD_MS_DEFAULT`).
- UART5 PAT5 parser runs in transport layer (`pat_uart5_pat5.c`), dispatcher in `main`.
- UART7 laser worker:
  - RX: `HAL_UARTEx_ReceiveToIdle_DMA`
  - Bypass command (`0x0004`) and cached status (`0x0005`)
  - status cache served to UART5 without per-request UART7 round-trip.

## Command map (implemented)

- `0x0004`: UART7 bypass exchange
- `0x0005`: get cached laser status
- `0x0200`: write full 32-byte MEMS register block
- `0x0201`: set `fc_hz`
- `0x0202`: set `ctrl`
- `0x0203`: get MEMS state + parser counters

## Validation checklist (software-verifiable)

- [x] Build passes: `cmake --build cmake-build --target pat_nucleo_mems_bringup`
- [x] New target links HAL SPI/UART/TIM/DMA stack and emits `.bin`
- [x] PAT5 transport parser compiles with CRC32 and length guards
- [x] UART7 DMA+IDLE path compiles and IRQ handlers are present in bring-up target
- [x] Pin constants consumed via `pat_pinmap.h` (no duplicated GPIO literals for new path)

## Bench checklist (hardware)

- [ ] Measure `FCLK_X` on `PC9` and `FCLK_Y` on `PA8` for default frequency and duty.
- [ ] Verify SPI5 waveform polarity/phase (Mode 1) and first SCK edge timing vs !CS.
- [ ] Validate UART5 at 921600 baud error budget against PolarFire endpoint.
- [ ] Verify UART7 bypass and cached status readback against the laser module protocol.
- [ ] Confirm PB8/PB9 polarity and direction from frozen HAT + laser interface specification.
