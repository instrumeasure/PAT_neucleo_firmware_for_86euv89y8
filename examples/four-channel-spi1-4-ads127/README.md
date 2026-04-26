# Four-channel SPI1–4 + ADS127L11 (quartet read)

Firmware target: **`pat_nucleo_quartet`** (`src/main_quartet.c`). One **epoch** = one sample vector **`raw[4]`** for logical ch0..3 (SPI1..SPI4 per [`AGENTS.md`](../../AGENTS.md) `quartet_order`). The quartet image **always** uses shared **`!CS`** (**`PAT_QUARTET_PARALLEL_DRDY_WAIT=1`**): all four **`!CS`** are **low together** for the DRDY wait and sample phase, then deasserted (4-wire). Per-channel sequential quartet (`ads127_read_sample24_ch_blocking` in a loop) is **not** a shipped mode. No RTOS.

### Parallel sample phase: register SPI (default) vs HAL IT (bisect)

- **Default (`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER=ON`):** after the shared DRDY gate, the 3-byte read uses **`pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`** — **interleaved** `TXDR`/`RXDR` polling on **SPI1..SPI4** so **SCLK can overlap on all buses** without **`HAL_SPI_TransmitReceive_IT`** or SPI NVIC for data (see [`src/pat_spi_h7_master.c`](../../src/pat_spi_h7_master.c), [`src/ads127l11.c`](../../src/ads127l11.c) `read_quartet_blocking_parallel`). RREG/WREG use **`pat_spi_h7_master_txrx`** unless the image is built with **`-DPAT_ADS127_SPI_HAL_LEGACY=ON`** (same CMake option as other PAT ELFs).
- **Bisect / legacy parallel IT:** configure CMake with **`-DPAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER=OFF`** then rebuild — restores **`HAL_SPI_TransmitReceive_IT`** + **`pat_quartet_spi_irq.c`** NVIC path.

**LA sign-off (parallel default):** during the post-DRDY 3-byte window, verify **SCLK on SPI1, SPI2, SPI3, and SPI4** all toggle in a **common time window** (compare to a capture with **`REGISTER_MASTER=OFF`** on the same HAT). UART **`EPOCH` / `epoch_hz_est`**, **`quartets_ok`**, and per-channel **`to` / `arm_skip`** should stay in the same class as the IT baseline.

**LA looks “dead” / no SCLK:** (1) Trigger on **any** `!CS` (all four fall together in parallel 4-wire) — SCLK is **bursty** (~24 edges per 3-byte read), not a free-running clock. (2) The epoch always gates on **SPI4 !DRDY** read from **PE15** (duplicate MISO net); if **PE15** is floating or wrong net, **`QUARTET_DRDY_TIMEOUT_MS`** loops with **no** sample SCLK. (3) Confirm VCP: **`to`** (DRDY timeout) and **`quartet_fail_total`** vs **`quartets_ok_total`**.

**DRDY gate:** **SPI4** only — **`SPE` off** on SPI4, **PE15** `IDR` poll (PE13 MISO stays AF5); UART **`arm_skip` on ch0–2** is always **0** (see `read_quartet_blocking_parallel` in [`src/ads127l11.c`](../../src/ads127l11.c)).

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
- **Nominal ODR:** see [`AGENTS.md`](../../AGENTS.md) `runtime` TOON (~24.4 ksps class at 25 MHz modulator clock, wideband OSR512); exact value from SBAS946 tables ± crystal tolerance.

## Epoch rate vs nominal ODR (LA + UART)

- **Nominal ODR** (~24.4 kS/s per ADS127 with default external CLK + OSR512 wideband) is set by the **converter**, not the UART summary period (`PAT_QUARTET_SYNC_SUMMARY_MS`).
- **Achieved quartet epoch rate** is how fast `ads127_read_quartet_blocking` returns in [`main_quartet.c`](../../src/main_quartet.c): there is **no TIM6** gate in the quartet image, so the main `for (;;)` loop runs **one epoch per iteration**. If SPI + DRDY polling + HAL overhead exceed **~20.5 µs**, the **epoch rate** falls below ODR (e.g. logic analyser on **`!CS`** in 4-wire may show **~20–25 kHz** edge rate ≈ **two edges per ~45 µs epoch**).
- **UART:** each `EPOCH,...` line includes **`span_us`** (DWT delta around `ads127_read_quartet_blocking`) and **`epoch_hz_est`** ≈ **1e6 / span_us** (integer Hz). Compare **`epoch_hz_est`** to your LA when the probe marks **one event per epoch** (e.g. one **!CS** falling edge per channel per epoch in parallel 4-wire).
- **LA probe cheat sheet (J1, parallel 4-wire):** all four **`!CS`** (PA4, PB4, PA15, PE11) fall together at epoch start and rise together at end; **SCLK** on each bus is **bursty** (24 clocks per 3-byte read), not a continuous ~24.4 kHz square wave; **DRDY/SDO** behaviour depends on **`SDO_MODE`** (see TI SBAS946).
- **CMake throughput / characterisation** (quartet target only; re-run CMake after changing cache):
  - **`PAT_SPI123_PRESCALER_DIV`**: `8` \| `16` \| `32` \| `64` (default **64**) — raises SPI1–3 SCLK vs [`pat_spi_ads127.c`](../../src/pat_spi_ads127.c); respect **TI f_SCLK** and SI on the HAT.
  - **`PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY`**: **0–15** (default **6**); lower numeric value = **higher** NVIC urgency on STM32.
  - **`PAT_QUARTET_DIAG_EPOCH_EVERY=ON`**: prints **`CNT` / `EPOCH` / `CH` every epoch** (UART flood) to correlate with LA without changing `PAT_QUARTET_SYNC_BURST_EPOCHS`.
  - **`PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED=ON`**: uses **sequential** `spi_master_rx3_zero_tx_unlocked` on SPI1..4 (no parallel IT overlap) — **diagnostic**. Takes precedence over **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER`** for the sample phase.
  - **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER`**: **ON** (default) = parallel **interleaved register** 3-byte sample; **OFF** = **`HAL_SPI_TransmitReceive_IT`** + SPI IRQ.
- **Tier B (DMA):** not implemented; gates and alignment notes in [`include/pat_quartet_p4_dma.h`](../../include/pat_quartet_p4_dma.h).

## GPIO `!CS`

By default all four **`!CS`** lines are **GPIO outputs, idle high** in **`ads127_pins_init()`** before SPI is used (exception: **`PAT_ADS127_SPI_3WIRE_CS_HELD_LOW`** holds them **low** — see [`AGENTS.md`](../../AGENTS.md) `quartet_ti_3wire_spi`).

## STM32H753 silicon notes

- **SPI2 MISO = PC2** (CubeMX often shows **PC2_C**; HAL: **GPIOC** `GPIO_PIN_2`): `ads127_pins_init()` configures **`SYSCFG_PMCR.PC2SO`** (default **closed** for the digital ball; see ST H7 FAQ). If channel 1 fails, try **`-DPAT_SPI2_PC2SO_OPEN_INSTEAD`**.
- **`!CS` on PA15 (SPI3) and PB4 (SPI2):** these balls are also JTAG/SWJ-related. Use Cube **SYS → Debug: Serial Wire** (SWD on PA13/PA14 only) in production builds so **PA15/PB4** can be driven as GPIO **CS**; keep ST-Link SWD-only wiring.

## SPI init parity

Shared helper **[`pat_spi_ads127.c`](../../src/pat_spi_ads127.c)** / **[`include/pat_spi_ads127.h`](../../include/pat_spi_ads127.h)** applies one **template** for **`SPI_InitTypeDef`** on **SPI1–SPI4** (CPOL/CPHA, prescaler, FIFO, NSS soft, etc.); only **`Instance`** differs. Each handle is **zero-initialised** before **`HAL_SPI_Init`** so **`HAL_SPI_MspInit`** runs per peripheral (see **stm32cube-hal-model** skill).

## Epoch cache and UART (machine-readable)

- **Published epoch line:** [`include/pat_quartet_epoch.h`](../../include/pat_quartet_epoch.h) — `pat_quartet_epoch_line_t` (32-byte aligned `raw24[4][3]` + `epoch_id` / `valid`) for downstream batched processing.
- **Boot / bring-up:** `BRU`, `SH`, `TI`, `STAT` CSV-style lines (see `pat_quartet_app.c`).
- **Runtime (throttled, default `PAT_QUARTET_SYNC_SUMMARY_MS` 1000 ms):** `CNT`, `EPOCH` (includes `span_us`, **`epoch_hz_est`**, DWT cycle delta), `CH` per channel with `st` / `to` / `arm_skip`. **`summary_ms` is UART cadence only**, not ADC ODR. Optional burst: define **`PAT_QUARTET_SYNC_BURST_EPOCHS`** > 0 at compile time, or CMake **`PAT_QUARTET_DIAG_EPOCH_EVERY=ON`** for every-epoch lines.
- **Tier B DMA (not enabled):** checklist header [`include/pat_quartet_p4_dma.h`](../../include/pat_quartet_p4_dma.h) — see § *Epoch rate vs nominal ODR* above.

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
