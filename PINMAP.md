# PAT Nucleo-H753ZI ↔ legacy HAT **86euv89y8** — pin map

**Board:** NUCLEO-H753ZI  
**HAT MCU link:** **J1** (ADC HAT) and **J2** (inter-HAT SPI). This map matches revision **86euv89y8** only; **86ex5v90x** swaps START/RESET vs `!ADC_EN` on J1 — do not mix semantics.

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
| MISO / SDO (DRDY poll pad) | **PB14** | AF5 SPI2 |

### Channel 2 — SPI3

| Signal | MCU pin | AF |
|--------|---------|-----|
| !CS (GPIO) | **PA15** | — |
| SCK | **PC10** | AF6 SPI3 |
| MISO / SDO (DRDY poll on same pad) | **PC11** | AF6 SPI3 |
| MOSI | **PD6** | AF5 SPI3 |

### Channel 3 — SPI4

| Signal | MCU pin | AF |
|--------|---------|-----|
| !CS (GPIO) | **PE11** | — |
| SCK | **PE6** | AF5 SPI4 |
| MOSI | **PE12** | AF5 SPI4 |
| MISO / SDO (DRDY poll pad) | **PE13** | AF5 SPI4 |

## J2 — SPI6 slave (QPD / host), AF8

| Signal | MCU pin | AF |
|--------|---------|-----|
| SCK (input) | **PA5** | AF8 SPI6 |
| NSS (input) | **PG8** | AF8 SPI6 |
| MISO (out) | **PG12** | AF8 SPI6 |
| MOSI (in) | **PG14** | AF8 SPI6 |

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

STM32CubeMX project: **`cube/legacy_86euv89y8_h753zi.ioc`** — authoritative when it disagrees with this table.

## References

- TI ADS127L11: [SBAS946](https://www.ti.com/lit/ds/symlink/ads127l11.pdf)  
- STM32H753: RM0433 / DS
