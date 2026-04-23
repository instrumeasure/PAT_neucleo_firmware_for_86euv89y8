---
name: four-channel-spi-ads127-quartet
description: >-
  PAT NUCLEO-H753ZI + four TI ADS127L11 on SPI1–SPI4 (86euv89y8 J1): quartet epoch
  SPI1→SPI2→SPI3→SPI4, ads127_read_quartet_blocking, per-channel ads127_ch_ctx_t,
  four !CS GPIO idle high, SPI Init parity, bare-metal cooperative scheduling from
  main (no RTOS). Triggers: quartet, SPI1 SPI2 SPI3 SPI4, four ADS127, AGENTS
  quartet_order, pat_nucleo_quartet.
metadata:
  pattern: tool-wrapper
  version: "1.0"
---

# Four-channel SPI1–4 + ADS127L11 (quartet)

## Before acting

1. **[`AGENTS.md`](../../AGENTS.md)** — `quartet_order`, J1 **86euv89y8**, shared START/RESET/CLK.
2. **[`examples/four-channel-spi1-4-ads127/README.md`](../../examples/four-channel-spi1-4-ads127/README.md)** — wiring, **`START_MODE` `00b` vs `10b`**, flash line for **`pat_nucleo_quartet.elf`**.
3. **[`examples/single-bus-spi1-4-ads127/README.md`](../../examples/single-bus-spi1-4-ads127/README.md)** — one-ADC-per-bus **example codebase** (`pat_nucleo_spiN_ads127`).
4. **[`single-channel-spi4-ads127`](../single-channel-spi4-ads127/SKILL.md)** — DRDY poll, **`ads127_read_sample24_*`**, bring-up semantics (this app extends to four buses).
5. **[`spi2-pc2c-miso-h7-pat`](../spi2-pc2c-miso-h7-pat/SKILL.md)** — **logical ch1** / **SPI2** **PC2SO**, **`MasterKeepIOState`**, when **ch1** mis-reads vs LA.
6. **[`stm32cube-hal-model`](../stm32cube-hal-model/SKILL.md)** — one **`HAL_SPI_*` owner** per handle from foreground in v1; **`HAL_SPI_Init`** after handle **`RESET`** so **`MspInit`** runs per instance.
7. **[`stm32h7-hal-pitfalls`](../stm32h7-hal-pitfalls/SKILL.md)** — **`PAT_SystemClock_Config`** includes **`HAL_InitTick`**.

## Software map

| Path | Role |
|------|------|
| [`src/main_quartet.c`](../../src/main_quartet.c) | USART3, **`ads127_pins_init`** then **`MX_SPI_All_Init`** (template parity), shared **`ads127_nreset_pulse`**, **`ads127_bringup`** ×4, **`ads127_read_quartet_blocking`**, printf ~1 Hz |
| [`src/ads127l11.c`](../../src/ads127l11.c) | **`ads127_ch_ctx_bind`**, **`ads127_read_sample24_ch_blocking`**, **`ads127_read_quartet_blocking`**, CS/MISO per context |
| [`src/stm32h7xx_hal_msp.c`](../../src/stm32h7xx_hal_msp.c) | **`HAL_SPI_MspInit`** branches SPI1–SPI4 |
| [`CMakeLists.txt`](../../CMakeLists.txt) | Target **`pat_nucleo_quartet`** (`APP_QUARTET_SRC`) |

## Post-START gate (shared START)

- Per channel: **`ads127_post_start_gate(hs[c], &sh[c])`** after bring-up (non-strict: failures print **`WARNING`** only).
- **Shared `PF1` START:** if **any** channel’s gate fails, firmware calls **`ads127_after_failed_post_start_gate()`** once (STOP/START pulse + **`ADS127_START_STREAM_SETTLE_MS`**) before **`ads127_read_quartet_blocking`** — see `src/main_quartet.c` and `src/ads127l11.h` / `ads127l11.c`.

## HAL invariants (v1)

- **No overlapping `HAL_SPI_TransmitReceive`** on the same **`hspiN`** from ISR without IRQ-safe HAL usage — quartet stays on **main** stack only.
- **SPI `Init` parity:** same template fields on SPI1–4; only **`Instance`** differs.
- **Four `!CS`:** all configured as **GPIO push-pull high** before SPI traffic in **`ads127_pins_init`**.

## Build / flash

[`stm32cube-cmake-pat`](../stm32cube-cmake-pat/SKILL.md); quartet ELF:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -Elf cmake-build/pat_nucleo_quartet.elf
```
