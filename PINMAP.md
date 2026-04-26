# PAT Nucleo-H753ZI ↔ legacy HAT **86euv89y8** — pin map

**Board:** NUCLEO-H753ZI  
**HAT MCU link:** **J1** (ADC HAT) and **J2** (inter-HAT SPI). This map matches revision **86euv89y8** only; **86ex5v90x** swaps START/RESET vs `!ADC_EN` on J1 — do not mix semantics.

**Locked SPI1–4 routing (authoritative in firmware):** `include/pat_pinmap.h` — change GPIO/AF there and keep this document + `cube/legacy_86euv89y8_h753zi.ioc` aligned.

## J1 — SPI summary (!CS, SCK, MOSI, MISO)

| SPI# | !CS | SCK | MOSI | MISO (SDO / !DRDY) |
|------|-----|-----|------|---------------------|
| SPI1 | **PA4** | **PG11** | **PD7** | **PG9** |
| SPI2 | **PB4** | **PB10** | **PB15** | **PC2** (CubeMX **PC2_C**) |
| SPI3 | **PA15** | **PC10** | **PD6** | **PC11** |
| SPI4 | **PE11** | **PE12** | **PE6** | **PE13** |

## J1 — shared control + four independent SPI buses (ADS127L11)

| Function | MCU pin | HAT / note |
|-----------|---------|------------|
| START (all channels) | **PF1** | J1-26, pin 16 all ch |
| nRESET (all channels) | **PF0** | J1-27, pin 6 |
| ADS127 CLK | (from HAT) | MCU does not drive modulator clock unless you add MCO/TIM |

### Channel 0 — SPI1

| Signal | MCU pin | AF |
|--------|---------|-----|
| !CS (GPIO) | **PA4** | — |
| SCK | **PG11** | AF5 SPI1 |
| MOSI | **PD7** | AF5 SPI1 |
| MISO / SDO (DRDY poll pad) | **PG9** | AF5 SPI1 |

### Channel 1 — SPI2

| Signal | MCU pin | AF |
|--------|---------|-----|
| !CS (GPIO) | **PB4** | — |
| SCK | **PB10** | AF5 SPI2 |
| MOSI | **PB15** | AF5 SPI2 |
| MISO / SDO (DRDY poll pad) | **PC2** (CubeMX **PC2_C**) | AF5 SPI2 |

**MCU note (STM32H753):** On **STM32CubeMX** this package pin is often named **PC2_C** (dual-pad / analog-switch); in **HAL/CMSIS** it is still **PC2** on **GPIOC** (`GPIO_PIN_2`) — same ball. Firmware sets **`SYSCFG_PMCR.PC2SO`** in `ads127_pins_init()` so the external ball is switched for digital SPI (see RM0433 *SYSCFG_PMCR*). If SPI2 MISO reads **0x000000** or never matches the LA on that pin, re-check HAT net, **AF5** on **GPIOC2**, and (package-dependent) whether **`PC2SO` should be set or cleared** — default **`PC2SO` closed** (digital path); optional **`-DPAT_SPI2_PC2SO_OPEN_INSTEAD`** sets the switch **open** if your package needs the previous behaviour. **`!CS`** on **PA15** / **PB4** is **`HAL_GPIO_DeInit` then per-pin `HAL_GPIO_Init`** so those balls leave reset **JTAG/SWJ** alternate functions before driving **GPIO** chip-select.

### Channel 2 — SPI3

| Signal | MCU pin | AF |
|--------|---------|-----|
| !CS (GPIO) | **PA15** | — |
| SCK | **PC10** | AF6 SPI3 |
| MOSI | **PD6** | AF5 SPI3 |
| MISO / SDO (DRDY poll on same pad) | **PC11** | AF6 SPI3 |

### Channel 3 — SPI4

| Signal | MCU pin | AF |
|--------|---------|-----|
| !CS (GPIO) | **PE11** | — |
| SCK | **PE12** | AF5 SPI4 |
| MOSI | **PE6** | AF5 SPI4 |
| MISO / SDO (DRDY poll pad) | **PE13** | AF5 SPI4 |
| SDO/!DRDY sense (duplicate MISO net, GPIO input for DRDY arm) | **PE15** | — |

## J2 — SPI6 slave (QPD / host), AF8

| Signal | MCU pin | AF |
|--------|---------|-----|
| SCK (input) | **PA5** | AF8 SPI6 |
| NSS (input) | **PG8** | AF8 SPI6 |
| MISO (out) | **PG12** | AF8 SPI6 |
| MOSI (in) | **PG14** | AF8 SPI6 |

Firmware smoke image (this branch): CMake target **`pat_nucleo_spi6`** — 64-byte SPI6 slave IT, USART3 heartbeat. Flash `cmake-build/pat_nucleo_spi6.elf` when exercising J2 (default **`pat_nucleo_h753.elf`** is still SPI4 + ADS127 milestone).

### SPI6 test frame (`pat_nucleo_spi6`, 64 bytes per NSS burst)

After each completed 64-byte transfer, the slave builds the **next** TX buffer from [`spi6_test_frame_fill`](include/spi6_test_frame.h) (`src/spi6_test_frame.c`):

| Offset | Content |
|--------|---------|
| 0–3 | `uint32_t` sequence **LE** = `completed_frame_index + 1` (1-based count since boot). Before the first master burst, idle fill uses **0xFFFFFFFF**. |
| 4 | Test format tag **0xA5** (not production QPD **0x02**). |
| 5–7 | First three bytes **MOSI** from the master on the transfer that just completed (`rx[0..2]`). |
| 8–63 | `tx[i] = (uint8_t)(seq ^ (i * 0x1D))` with `seq` as above. |

## Debug / status

| Function | MCU pin |
|----------|---------|
| USART3 TX | **PD8** (ST-Link VCP) |
| USART3 RX | **PD9** |
| Baud | 115200 8N1 |
| User LED1 | **PB0** |
| User LED2 | **PB7** |

## NVIC order (reference for refactor)

Prior working image used: **SPI1–4 IRQ = 3**, **TIM6_DAC = 4**, **SPI6 = 6** (ADC completions above sample gate and above SPI6 slave load).

## Machine-readable pinout

- **`include/pat_pinmap.h`** — SPI1–4 + shared START/RESET (used by firmware).
- **`src/stm32h7xx_hal_msp.c`** — `HAL_SPI_MspInit` / `HAL_SPI_MspDeInit` for SPI1–4 and USART3 (single implementation; do not duplicate in other `.c` files).
- **`cube/legacy_86euv89y8_h753zi.ioc`** — Cube snapshot; keep in sync when changing AF routing.

## References

- TI ADS127L11: [SBAS946](https://www.ti.com/lit/ds/symlink/ads127l11.pdf)  
- STM32H753: RM0433 / DS
