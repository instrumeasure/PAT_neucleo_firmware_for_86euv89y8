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

## J2 — SPI6 slave (PolarFire / host), AF8

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

### J2 — UART5 command bridge (PolarFire → MCU), system design

On the **MCU HAT**, **J2** also carries **UART5** to the **PolarFire** HAT for **command / telemetry** (stack integration). The **Nucleo receives host commands on UART5 RX** (**PD2**); **TX** is **PC12**. Typical line rate in system models: **921600** 8N1 — confirm against final PolarFire RTL/driver.

| Signal | MCU pin | AF |
|--------|---------|-----|
| UART5 TX (MCU → PolarFire) | **PC12** | AF8 UART5 |
| UART5 RX (PolarFire → MCU) | **PD2** | AF8 UART5 |

**Firmware status (this repo):** **`UART5` is not initialised** in [`stm32h7xx_hal_msp.c`](src/stm32h7xx_hal_msp.c); bring-up and logs use **USART3** (**PD8**/**PD9**, ST-Link VCP, **115200**). Adding PolarFire command RX requires **`MX_UART5_Init`**, NVIC, and an IT/DMA RX path — track as a separate change. **Payload framing (magic `PAT5`, CRC-32, opcode bands):** [`docs/UART5_POLARFIRE_PAYLOAD.md`](docs/UART5_POLARFIRE_PAYLOAD.md). System-design cross-reference: **§ uart5** / MCU HAT **J2-10**/**J2-12** in **02b1-mcu-pinmap** (LEO laser-comm stack).

### UART7 — laser driver (system design)

**UART7** is the **MCU host** link to the **laser driver** (beacon / TO56 butterfly path in the stack). It is **separate** from **UART5** (PolarFire): different pins, different partner device.

| Signal | MCU pin | AF (verify in Cube / DS) |
|--------|---------|---------------------------|
| UART7 TX (MCU → laser driver) | **PE8** | AF7 UART7 |
| UART7 RX (laser driver → MCU) | **PE7** | AF7 UART7 |
| `laser_driver_oc` (digital control connector pin 7) | **PB8** | GPIO (AF n/a) |
| `int_lock` (digital control connector pin 6) | **PB9** | GPIO (AF n/a) |

**Vendor protocol / electrical reference (SF8XXX TO56B family):** [Laser Diode Control — SF8XXX_TO56B manual (PDF)](https://www.laserdiodecontrol.com/files/manuals/laserdiodecontrol_com/10237/SF8XXX_TO56B_Manual-1720730740.pdf). Planning notes and bypass bridge from **UART5**: [`docs/UART7_LASER_DRIVER.md`](docs/UART7_LASER_DRIVER.md). **Baud, line idle, and command framing** follow that document until firmware freezes one profile — **UART5** carries **PAT5** only; laser-native bytes use **`LASER_UART7_BYPASS`** in [`docs/UART5_POLARFIRE_PAYLOAD.md`](docs/UART5_POLARFIRE_PAYLOAD.md) **§8**.

Connector label in pin-map docs is often **“Beacon link”** — map to your harness. The same harness exposes a **digital control connector** carrying **PB8** (`laser_driver_oc`) and **PB9** (`int_lock`) above.

**PB8/PB9 semantics are TBD until hardware freeze:** confirm **polarity** and **direction** (MCU input vs output) from the final HAT schematic + SF8xxx control docs before enabling active logic in firmware.

**Firmware status (this repo):** **`pat_nucleo_mems_bringup`** initialises **`UART7`** with **DMA + UART IDLE RX** and a light vendor parser in `main`; default legacy targets still boot with USART3-only diagnostics.

### SPI5 + MEMS drive (bring-up target)

| Signal | MCU pin | AF |
|--------|---------|----|
| SPI5 `!SYNC` / !CS (GPIO) | **PF6** | GPIO |
| SPI5 SCK | **PF7** | AF5 SPI5 |
| SPI5 MOSI | **PF9** | AF5 SPI5 |
| FCLK_X PWM | **PC9** | AF2 TIM3_CH4 |
| FCLK_Y PWM | **PA8** | AF1 TIM1_CH1 |
| MEMS EN | **PA9** | GPIO |

**Current firmware choice:** one PWM channel per pin due AF split on this board profile (`TIM3_CH4` for **PC9**, `TIM1_CH1` for **PA8**), both clocked from the same programmed base frequency in `pat_nucleo_mems_bringup`.

### Stack UART summary (production wiring)

| UART | Partner | Typical role |
|------|-----------|----------------|
| **UART5** | **PolarFire** (MCU HAT **J2**) | Command / telemetry bridge |
| **UART7** | **Laser driver** | Beacon / laser control |

**USART3** (**PD8**/**PD9**) remains **development** (ST-Link VCP) unless firmware policy routes diagnostics elsewhere.

### I²C — fibre converter (signal intensity)

The stack exposes **I²C** as **MCU master** to a **fibre-optic converter** front-end so firmware can **read signal intensity** (and any other registers the converter exposes — exact part / map TBD).

| Signal | MCU pin | AF |
|--------|---------|-----|
| **I2C1 SCL** | **PB6** | AF4 I2C1 |
| **I2C1 SDA** | **PB7** | AF4 I2C1 |

**Connector inventory (system design):** MCU HAT **J9** — **I²C breakout** (**4-pin JST SH**), **`i2c1` PB6/PB7** with pull-ups — see **02b4-connector-inventory** in the Leo laser-comm system-design pack.

**Integration caveat:** **PB7** is listed here as **I²C1_SDA** on the stack **J9** net; **this repo’s Nucleo table** also uses **PB7** for **LD2**. On a bare **NUCLEO-H753ZI**, resolve **LED vs I²C** in **MCU HAT** routing or accept **GPIO** clash — **CubeMX / final schematic** wins.

**Firmware status (this repo):** **`I2C1` not initialised** (HAL I²C compiled in [`stm32h7xx_hal_conf.h`](src/stm32h7xx_hal_conf.h) only). Add **`HAL_I2C_MspInit`** for **PB6**/**PB7**, device **7-bit address** and register map per the converter datasheet.

### Stack buses summary (beyond **J1** ADC SPI)

| Bus | Partner / target | Role |
|-----|-------------------|------|
| **UART5** | PolarFire (**J2**) | Commands / telemetry |
| **UART7** | Laser driver | Beacon / SF8xxx TO56B class — [vendor manual (PDF)](https://www.laserdiodecontrol.com/files/manuals/laserdiodecontrol_com/10237/SF8XXX_TO56B_Manual-1720730740.pdf) |
| **I²C1** | Fibre converter | Signal **intensity** readout (+ vendor regs) |

## Debug / status

| Function | MCU pin |
|----------|---------|
| USART3 TX | **PD8** (ST-Link VCP) |
| USART3 RX | **PD9** |
| Baud | 115200 8N1 |
| User LED1 | **PB0** |
| User LED2 | **PB7** (see **I²C** § — **PB7** may be **I2C1_SDA** on stack **J9**) |

Production wiring: **UART5** ↔ **PolarFire** (**J2**), **UART7** ↔ **laser driver**, **I²C1** ↔ **fibre converter** (intensity) — see § above; **USART3** remains **lab / dev** unless you deliberately mirror or replace it in firmware.

## NVIC order (reference for refactor)

**Policy:** **`SPI1_IRQn` … `SPI4_IRQn`** and **`SPI6_IRQn`** sit in the **tightest** (most pre-emptive) priority group used for measurement — **UART5**, **SPI5** (MEMS), and other control ISRs must **not** pre-empt them. Non-SPI work should stay **short** in ISR and defer parsing / heavy logic to **main** when SPI paths are idle.

Prior working image used: **SPI1–4 IRQ = 3**, **TIM6_DAC = 4**, **SPI6 = 6** (ADC completions above sample gate and above SPI6 slave load). When adding **UART5** or **MEMS**, assign **higher numeric values** (looser preemption) than **SPI1–4** and **SPI6** unless profiling shows a safe alternative — see [docs/UART5_POLARFIRE_PAYLOAD.md](docs/UART5_POLARFIRE_PAYLOAD.md) §7.1.

## Machine-readable pinout

- **`include/pat_pinmap.h`** — SPI1–6 + UART5/UART7 + MEMS/laser GPIO constants.
- **`src/stm32h7xx_hal_msp.c`** — SPI1–6, USART3, UART5, UART7, TIM1/TIM3 PWM post-init for MEMS FCLK.
- **`cube/legacy_86euv89y8_h753zi.ioc`** — Cube snapshot; keep in sync when changing AF routing.

## References

- TI ADS127L11: [SBAS946](https://www.ti.com/lit/ds/symlink/ads127l11.pdf)  
- STM32H753: RM0433 / DS
