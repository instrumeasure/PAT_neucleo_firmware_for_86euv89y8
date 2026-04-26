# Example codebase: one ADS127L11 per SPI bus (SPI1–SPI4)

This document is the **canonical reference** for the **single-bus** PAT builds: four separate ELFs, one source tree (`src/main_single_ads127_spi.c`), compile-time selection of **which SPI** talks to **one** ADS127 on the **86euv89y8** J1 HAT. Same ADC protocol and driver as the default SPI4 app (`main.c` / `pat_nucleo_h753`), without quartet scheduling.

## What you get

| CMake target | Flash flag | `PAT_ADS127_SINGLE_SPI_BUS` | Logical ch (`PAT_LOG_CH`) |
|--------------|------------|----------------------------|----------------------------|
| `pat_nucleo_spi1_ads127` | `-SingleSpi 1` | 1 | 0 |
| `pat_nucleo_spi2_ads127` | `-SingleSpi 2` | 2 | 1 |
| `pat_nucleo_spi3_ads127` | `-SingleSpi 3` | 3 | 2 |
| `pat_nucleo_spi4_ads127` | `-SingleSpi 4` | 4 | 3 |

All targets link **`main_single_ads127_spi.c`** + **`ads127l11.c`** + **`pat_spi_h7_master.c`** + HAL/MSP/clock (`CMakeLists.txt` `foreach` over `bus` 1..4).

### HAL-free SPI path (default)

RREG/WREG and the **3-byte** sample burst use **`pat_spi_h7_master_txrx`** (H7 SPI v2 `TSIZE`/`CSTART`/EOT polling, **DWT** timeouts) instead of **`HAL_SPI_TransmitReceive`**, while **`HAL_SPI_Init`** + **`HAL_SPI_MspInit`** still configure the peripheral. MISO **GPIO ↔ AF** for SDO/DRDY remains **register-only** in **`ads127l11.c`**.

### Legacy SPI (bisect)

Reconfigure CMake with **`-DPAT_ADS127_SPI_HAL_LEGACY=ON`** then rebuild — restores **`HAL_SPI_TransmitReceive`** for register frames and the old 3-byte inner loop (`HAL_GetTick` timeouts). Use to compare **`drdy_timeouts`** / **`arm_skip`** (UART) or LA on **!CS → first SCLK** vs default.

## Hardware (J1)

- **MCU:** NUCLEO-H753ZI. **HAT:** legacy **86euv89y8** — **J1** (see **`PINMAP.md`** / **`include/pat_pinmap.h`**).
- **Per build:** only **one** SPI peripheral is clocked and used; the others’ `!CS` lines stay **GPIO idle high** from **`ads127_pins_init()`**.
- **Shared:** `PF0` nRESET, `PF1` START, modulator **CLK** from the HAT.

Pin hints printed at boot (see also **`pat_j1_hint()`** in source):

| Bus | !CS | SCK | MOSI | MISO |
|-----|-----|-----|------|------|
| SPI1 | PA4 | PG11 | PD7 | PG9 |
| SPI2 | PB4 | PB10 | PB15 | PC2 (`PC2SO` in `ads127_pins_init`) |
| SPI3 | PA15 | PC10 | PD6 | PC11 |
| SPI4 | PE11 | PE12 | PE6 | PE13 |

## Firmware layout (example structure)

| Path | Role |
|------|------|
| `src/main_single_ads127_spi.c` | `main()`: `PAT_ADS127_SINGLE_SPI_BUS` → instance, USART3, **`ads127_pins_init()` then `MX_SPI_Init()`**, bring-up retry, TI `t_c(SC)` check, STATUS RREG, **START** + **`ADS127_START_STREAM_SETTLE_MS`**, **`ads127_post_start_gate`**, non-strict **`ads127_after_failed_post_start_gate()`** on gate fail, tight **`ads127_read_sample24_blocking`** + ~1 Hz printf |
| `src/ads127l11.c` / `src/ads127l11.h` | RREG/WREG, shadow refresh, **`ads127_read_sample24_ch_blocking`** (CS → 100 ns → SPE off → MISO GPIO pull-up DRDY poll → AF → 24 SCLK burst), gate + recovery helpers |
| `src/pat_spi_h7_master.c` / `include/pat_spi_h7_master.h` | H7 SPI v2 polled **`pat_spi_h7_master_txrx`** (shared abort cleanup); **`pat_spi_h7_master_cfg_from_hspi`** after `HAL_SPI_Init` |
| `src/stm32h7xx_hal_msp.c` | `HAL_SPI_MspInit` for SPI1–SPI4 AF |
| `src/pat_clock.c` | PLL + `HAL_InitTick` |
| `CMakeLists.txt` | `APP_SINGLE_ADS127_SPI_SRC` + `target_compile_definitions(... PAT_ADS127_SINGLE_SPI_BUS=${bus})` |

**Driver wiring:** `ads127_read_sample24_blocking(hspi, …)` maps **`hspi->Instance`** to **`ads127_ch_ctx_t`** via **`ctx_pack_for_hspi`** → **`ads127_ch_ctx_bind`** so **!CS** / **MISO** / AF numbers match the active bus without duplicating sample logic in the main file.

## SPI and init invariants (same family as `pat_nucleo_h753`)

- **SPI mode 1** (HAL): CPOL low, CPHA 2nd edge; **`SPI_NSS_SOFT`**; **`MasterKeepIOState = ENABLE`** on SPI1–4 so MISO stays valid when **SPE** is off during DRDY poll.
- **Prescaler:** SPI4 build uses **÷16**; SPI1–3 use **÷64** (see `mx_spi_prescaler()`).
- **Order:** **`ads127_pins_init()`** before **`HAL_SPI_Init()`** — required for **SPI2** `SYSCFG_PMCR.PC2SO` and for de-init/re-init of **PA15** / **PB4** `!CS` pads away from JTAG AF.

## Build and flash

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -SingleSpi 3
```

Use **`-SingleSpi 1`** … **`-SingleSpi 4`** for the matching **`pat_nucleo_spiN_ads127.elf`**. See **`.cursor/skills/stm32cube-cmake-pat/SKILL.md`**.

**Strict bring-up (optional):** configure CMake with **`-DPAT_ADS127_STRICT_BRINGUP=ON`** then rebuild — firmware halts on bring-up or post-START gate failure instead of printing WARNING and streaming.

## Related examples and skills

- Default **SPI4 only** (same protocol, **`main.c`**): [single-channel-spi4-ads127](../single-channel-spi4-ads127/README.md) → **`pat_nucleo_h753.elf`**.
- **Sequential** SPI1→4 on one image (net check): [spi1-4-net-scan](../spi1-4-net-scan/README.md) → **`pat_nucleo_spi1_4_scan.elf`**.
- **Four ADCs in one loop:** [four-channel-spi1-4-ads127](../four-channel-spi1-4-ads127/README.md) → **`pat_nucleo_quartet.elf`**.
- Cursor: **`.cursor/skills/single-channel-spi4-ads127/SKILL.md`** (covers this flow + default `main.c`).

## References

- TI **ADS127L11** / **SBAS946**
- **`.cursor/rules/ads127l11-adc.mdc`**
- **`AGENTS.md`** — J1, `quartet_order`, heartbeat fields
