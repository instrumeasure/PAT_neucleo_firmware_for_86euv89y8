# Four-channel SPI1–4 + ADS127L11 (quartet read)

Firmware target: **`pat_nucleo_quartet`** (`src/main_quartet.c`). One **epoch** = one sample vector **`raw[4]`** read in fixed MCU order **SPI1 → SPI2 → SPI3 → SPI4** (same as [`AGENTS.md`](../../AGENTS.md) `quartet_order`). Only one `!CS` is active per SPI transaction; all `HAL_SPI_*` calls run from the main loop (cooperative “threads”, no RTOS).

## J1 routing (legacy HAT 86euv89y8)

| Logical ch | SPI | MCU `!CS` | SCK / MOSI / MISO (summary) |
|------------|-----|-----------|-----------------------------|
| 0 | SPI1 | PA4 | PG11 / PD7 / PG9 |
| 1 | SPI2 | PB4 | PB10 / PB15 / PC2 |
| 2 | SPI3 | PA15 | PC10 / PD6 / PC11 |
| 3 | SPI4 | PE11 | PE12 / PE6 / PE13 |

Shared nets: **PF0** `nRESET`, **PF1** **START** (all four converters). **CLK** is hat-generated (MCU does not drive modulator clock). Authoritative literals: [`include/pat_pinmap.h`](../../include/pat_pinmap.h), [`PINMAP.md`](../../PINMAP.md).

## ADS127 control (TI SBAS946)

- **Multi-device:** program **each** die on its own SPI/`!CS`; keep mode-related registers aligned where the data sheet requires it, then assert **START** once for the shared net.
- **`CONFIG2.START_MODE`:** **`00b`** = start/stop (START high runs conversions until low) — default in [`src/ads127l11.c`](../../src/ads127l11.c) bring-up. **`10b`** = *synchronised control*: rising edge on **START** aligns the digital filter; continuous conversion thereafter — use when you need TI’s synchronised-filter semantics, not the same as simple **`00b`** streaming.
- **Nominal ODR:** see [`AGENTS.md`](../../AGENTS.md) `runtime` TOON (~48.8 ksps class at 25 MHz modulator clock, wideband OSR256); exact value from SBAS946 tables ± crystal tolerance.

## GPIO `!CS`

All four **`!CS`** lines are configured as **GPIO outputs, idle high** in **`ads127_pins_init()`** before SPI is used, so inactive buses do not float **CS** during bring-up or single-bus builds.

## STM32H753 silicon notes

- **SPI2 MISO = PC2** (CubeMX often shows **PC2_C**; HAL: **GPIOC** `GPIO_PIN_2`): `ads127_pins_init()` configures **`SYSCFG_PMCR.PC2SO`** (default **closed** for the digital ball; see ST H7 FAQ). If channel 1 fails, try **`-DPAT_SPI2_PC2SO_OPEN_INSTEAD`**.
- **`!CS` on PA15 (SPI3) and PB4 (SPI2):** these balls are also JTAG/SWJ-related. Use Cube **SYS → Debug: Serial Wire** (SWD on PA13/PA14 only) in production builds so **PA15/PB4** can be driven as GPIO **CS**; keep ST-Link SWD-only wiring.

## SPI init parity

Shared helper **[`pat_spi_ads127.c`](../../src/pat_spi_ads127.c)** / **[`include/pat_spi_ads127.h`](../../include/pat_spi_ads127.h)** applies one **template** for **`SPI_InitTypeDef`** on **SPI1–SPI4** (CPOL/CPHA, prescaler, FIFO, NSS soft, etc.); only **`Instance`** differs. Each handle is **zero-initialised** before **`HAL_SPI_Init`** so **`HAL_SPI_MspInit`** runs per peripheral (see **stm32cube-hal-model** skill).

## Epoch cache and UART (machine-readable)

- **Published epoch line:** [`include/pat_quartet_epoch.h`](../../include/pat_quartet_epoch.h) — `pat_quartet_epoch_line_t` (32-byte aligned `raw24[4][3]` + `epoch_id` / `valid`) for downstream batched processing.
- **Boot / bring-up:** `BRU`, `SH`, `TI`, `STAT` CSV-style lines (see `pat_quartet_app.c`).
- **Runtime (throttled, default `PAT_QUARTET_SYNC_SUMMARY_MS` 1000 ms):** `CNT`, `EPOCH` (includes `span_us` from DWT cycle delta), `CH` per channel with `st` / `to` / `arm_skip`. **`summary_ms` is UART cadence only**, not ADC ODR. Optional burst: define **`PAT_QUARTET_SYNC_BURST_EPOCHS`** > 0 at compile time.
- **Tier B DMA (not enabled):** checklist header [`include/pat_quartet_p4_dma.h`](../../include/pat_quartet_p4_dma.h).

## API

- **`ads127_ch_ctx_bind`** — fills **`ads127_ch_ctx_t`** for channel index 0..3.
- **`ads127_read_sample24_ch_blocking`** — one device, explicit context.
- **`ads127_read_quartet_blocking`** — one epoch SPI1→SPI4; per-channel **`ads127_diag_t`**.

## Build

Same as default PAT CMake flow ([**stm32cube-cmake-pat**](../../.cursor/skills/stm32cube-cmake-pat/SKILL.md)):

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
```

Artifacts: **`cmake-build/pat_nucleo_quartet.elf`** and **`.bin`**.

## Flash (non-default ELF)

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -Quartet
# same image:
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -Elf cmake-build/pat_nucleo_quartet.elf
```

**Note:** With **no** arguments, `Flash-Stm32CubeOpenOCD.ps1` programs the **default** `pat_nucleo_h753` (single SPI4 app), not the quartet — use **`-Quartet`** or **`-Elf …quartet…`** so the four-channel build runs on the board.

Adjust the **`-Elf`** path if your build directory differs.

## References

- TI [ADS127L11 / SBAS946](https://www.ti.com/lit/ds/symlink/ads127l11.pdf)
- [`.cursor/rules/ads127l11-adc.mdc`](../../.cursor/rules/ads127l11-adc.mdc)
- Single-channel SPI4 default: [single-channel-spi4-ads127](../single-channel-spi4-ads127/README.md)
- Single-bus SPI1–4 ELFs (`main_single`): [single-bus-spi1-4-ads127](../single-bus-spi1-4-ads127/README.md)
- SPI1–4 sequential scan: [spi1-4-net-scan](../spi1-4-net-scan/README.md)
