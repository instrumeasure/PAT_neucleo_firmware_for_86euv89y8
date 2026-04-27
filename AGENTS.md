# PAT Nucleo firmware — agent guide

STM32H753ZI + TI ADS127L11×4 (legacy **86euv89y8** MCU↔ADC **J1** only). Authoritative onboarding; pins/code beat generic tutorials.

```toon
hardware[HAT,must_read]
86euv89y8,J1 only|PF1→J1-26 START (pin16 all ch)|PF0→J1-27 RESET (pin6)|SPI1..4=!CS per channel CS_1..4
86ex5v90x,SKIP until migrated|J1-26=!ADC_EN J1-27=START swaps vs legacy — do NOT assume PF0/PF1 semantics here
```

Refs: TI [SBAS946](https://www.ti.com/lit/ds/symlink/ads127l11.pdf) • STM32 RM/DS/errata/HAL for exact part.

Coding: HAL in-tree • minimal ISRs • `stdint` + TI register names • no silent 24→16-bit chop.

```toon
build[primary]
invoke,cmd
cmake_ninja,powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
flash_openocd,powershell -File scripts/Flash-Stm32CubeOpenOCD.ps1|default pat_nucleo_h753|four-channel -Quartet|SPI1-4 net scan -Spi1_4|-Spi123 alias|single bus N -SingleSpi 1..4 → pat_nucleo_spiN_ads127|SPI6 -Spi6
hal_pack,STM32_CUBE_H7_FW or %USERPROFILE%\.platformio\packages\framework-stm32cubeh7 (populate via py -m platformio run -e nucleo_h753zi once)
out,cmake-build/pat_nucleo_h753.{elf,bin}|also pat_nucleo_spi{1,2,3,4}_ads127.{elf,bin}
needs,CMake Ninja arm-none-eabi-gcc | skill,.cursor/skills/stm32cube-cmake-pat/SKILL.md
ads127_gate,default warn+stream|strict halt cmake -DPAT_ADS127_STRICT_BRINGUP=ON then rebuild
```

Optional: `platformio.ini` / `.cursor/skills/platformio-stm32-pat/SKILL.md` • Cube snapshot `cube/legacy_86euv89y8_h753zi.ioc` • timing/SPI changes → LA/scope CLK CS SCLK DRDY.

```toon
runtime[key]
sample_hz,24414|SAMPLE_RATE_HZ TIM6 when ADS127_CONFIG4_USER=0x80 (25 MHz FCLK) ≈ 50k×25/25.6/2 OSR512 wideband
sample_hz_alt,25000|if CONFIG4 internal (0x00) @ 25.6 MHz nominal OSR512 — use ADS127_ODR_HZ_NOMINAL_25M6_MHZ
ext_clk,ADS127_CONFIG4_USER=0x80 default in ads127l11.h (external master clock on CLK per SBAS946)
quartet_order,ch0..3=SPI1..SPI4|`pat_nucleo_quartet`: always **`PAT_QUARTET_PARALLEL_DRDY_WAIT=1`** (shared !CS + DRDY epoch; no per-channel sequential quartet in-tree). DRDY gate is always **SPI4 PE15** (duplicate SDO/!DRDY net; `ctxs[3]`). **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER=ON` (default)** → interleaved register quartet SPI (`pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`, no HAL SPI IT); **OFF** → `HAL_SPI_TransmitReceive_IT` + NVIC; UART milestones `main_quartet.c`; bring-up: one **nRESET** + `ads127_bringup_no_nreset` per bus
quartet_ti_3wire_spi,optional CMake|`PAT_ADS127_SPI_3WIRE_CS_HELD_LOW=ON` (quartet target only): SBAS946 §8.5.9 — all four !CS **held low** from GPIO init through nRESET so each ADS127 latches 3-wire (STATUS.CS_MODE=1); firmware never drives !CS high; frames delimited by SCLK count; see `ads127l11.c` / `main_quartet.c`
quartet_epoch_vs_odr,UART+LA|`EPOCH,...,epoch_hz_est` ≈ 1e6/`span_us` (MCU quartet **epoch** rate); nominal per-die ODR ~`sample_hz` (TIM6 note is `main.c` only); LA on **!CS** (4-wire) ≈ two edges per epoch on each chip-select; **SCLK** is bursty; CMake throughput knobs (quartet only): `PAT_SPI123_PRESCALER_DIV` 8|16|32|64, `PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY` 0–15, `PAT_QUARTET_DIAG_EPOCH_EVERY`, `PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED` (requires parallel ON); future DMA tier-B — [include/pat_quartet_p4_dma.h](include/pat_quartet_p4_dma.h), [examples/four-channel-spi1-4-ads127/README.md](examples/four-channel-spi1-4-ads127/README.md)
quartet_rolling_export,fmt0x0A 64B|`include/pat_quartet_rolling.h` offsets: `epoch_seq` LE [0..3], `[4]=fmt`, `[5]=dds_p`, `[6]=flags(bit0 mean_valid)`, `[7]=wpos`, means LE int32 raw/i/q [8..55], [56..63]=0; producer uses `p64[2][64]` fill+flip (`read_idx`)
quartet_rolling_margin,~20us class|On 86euv89y8 use !CS→!DRDY margin for `on_epoch`+fill+stage only; do not block next quartet read waiting for SPI6 wire completion
clk_source,HAT generates CLK → MCU does not drive unless you add MCO/TIM doc’d in code
qpd_dsp,3×16 ring per path raw|I|Q y=(sum)>>4|p nibble in SPI6 hdr
spi6_j2,PA5 SCK PG8 NSS PG12 MISO PG14 MOSI|SPI6 slave fmt0x02 64B LE sample_index+8B hdr+4×3×24b BE+pad|IT double-buffer
spi6_host,docs/SPI6_HOST_MANUAL.md|J2 **master** wiring, SPI **mode 1** (CPOL0 CPHA1), 64B/NSS, parse **fmt0x0A** rolling (see `quartet_rolling_export`) vs `0x02` QPD pack
spi6_sclk,master sets f_SCLK|target &gt; SPI1–4 and &gt; 14.4 MHz per system design or AGENTS waiver
```

```toon
review_packet[instrumentation §7.1]
probes,ground|scope BW vs SCLK|J1 J2 captures
fclk,25MHz ADS127|ODR vs TIM6|HB cadence
as_built,AGENTS line|f_SCLK J1+J2|T_xfer 64B|bench vs production HDL
```

```toon
review_packet[stm32 §7.2]
files,stm32h7xx_it.c|qpd_spi6_slave.c HAL_SPI_*Cplt|single callback owner
nvic,SPI1-4+SPI6 highest tier|UART5_MEMS_SPI5_lower_tier_main_when_idle|TIM6_DAC vs SPI6 typical 5 vs 6 in ref image
it_dma,SPI6 IT+static buffers|DMA only if D-cache rules met
pointers,stm32h7-hal-pitfalls|stm32-firmware.mdc
```

USART3 **PD8/PD9** → ST-Link VCP **115200**. **Production UARTs (two):** **UART5** (**PC12**/**PD2**, MCU HAT **J2**) ↔ **PolarFire** commands / telemetry (**921600** typical per **02b1-mcu-pinmap.md**); **UART7** (**PE8**/**PE7**) ↔ **laser driver** (MCU hosts link; protocol per module — e.g. [SF8XXX_TO56B manual PDF](https://www.laserdiodecontrol.com/files/manuals/laserdiodecontrol_com/10237/SF8XXX_TO56B_Manual-1720730740.pdf), see [PINMAP.md](PINMAP.md) § UART7). Laser digital control connector adds **PB8** `laser_driver_oc` (pin 7) and **PB9** `int_lock` (pin 6), GPIO semantics/polarity frozen in UART7 + pinmap docs. **I²C1** (**PB6**/ **PB7**, **J9** in stack inventory) ↔ **fibre converter** signal-intensity readout. **`pat_nucleo_mems_bringup` now initialises `UART5` + `UART7` (+ SPI5/TIM MEMS path)**; legacy acquisition targets remain USART3-centric unless extended. **UART5 wire format:** [docs/UART5_POLARFIRE_PAYLOAD.md](docs/UART5_POLARFIRE_PAYLOAD.md) (`PAT5` + **CRC-32** + `cmd` / `seq` / `flags`); **PolarFire → laser** opaque tunnel **`LASER_UART7_BYPASS` (0x0004, §8)**; laser **status** to PolarFire is **`GET_LASER_STATUS` (0x0005, §9)** — MCU keeps a **low-rate UART7** snapshot in RAM and answers **UART5** from cache (no per-query laser round-trip in v1). **UART7** planning / PDF link: [docs/UART7_LASER_DRIVER.md](docs/UART7_LASER_DRIVER.md) — **DMA + IDLE** RX, **vendor parser** in **main** (wire grammar from SF8xxx PDF, not PAT5).

```toon
heartbeat[CSV per line ending CRLF ~1Hz]
HB,state,tick_ms,quartets_ok,raw0,raw1,raw2,raw3
quartets_ok,successful full quartet epochs since boot `ads127_get_quartet_acquired_count()` in [`src/ads127l11.c`](src/ads127l11.c) (increments only when `ads127_read_quartet_blocking` returns `HAL_OK`; used by `pat_nucleo_quartet` path; default `main.c` heartbeat may omit)
raw_n,0x+6hex 24-bit|sentinel 0xFFFFFF invalid|same as printf in heartbeat_tick()
states,INIT ADC_CFG RUN ERR_SPI ERR_ADC|strings from app_state.c
```

```toon
rules_skills[path,role]
.cursor/skills/README.md,skill index read order matrix
.cursor/rules/stm32-firmware.mdc,STM32 C style
.cursor/rules/ads127l11-adc.mdc,TI wire/DRDY/SPI notes
.cursor/skills/stm32cube-cmake-pat/SKILL.md,default build
.cursor/skills/platformio-stm32-pat/SKILL.md,optional PIO
.cursor/skills/stm32h7-hal-pitfalls/SKILL.md,SysTick PLL tick
.cursor/skills/stm32cube-hal-model/SKILL.md,HAL model UM2217 MspInit State
.cursor/skills/single-channel-spi4-ads127/SKILL.md,single ADS127L11 DRDY poll pat_nucleo_h753 main_single pat_nucleo_spi1_4_ads127
.cursor/skills/spi2-pc2c-miso-h7-pat/SKILL.md,SPI2 PC2_C MISO PC2SO pat_nucleo_spi2_ads127 logical ch1
.cursor/skills/four-channel-spi-ads127-quartet/SKILL.md,SPI1-4 quartet ads127_read_quartet_blocking pat_nucleo_quartet
```

```toon
example_ref[single SPI4 + ADS127L11 ch3]
readme,examples/single-channel-spi4-ads127/README.md
index,examples/README.md
elf,cmake-build/pat_nucleo_h753.elf|APP_SRC main.c in root CMakeLists.txt
```

```toon
example_ref[quartet SPI1-4 + four ADS127L11]
readme,examples/four-channel-spi1-4-ads127/README.md
index,examples/README.md
elf,cmake-build/pat_nucleo_quartet.elf|APP_QUARTET_SRC main_quartet.c in root CMakeLists.txt
order,SPI1→SPI2→SPI3→SPI4|ads127_read_quartet_blocking
```

```toon
example_ref[single bus SPI1-4 one ADS127 per ELF]
readme,examples/single-bus-spi1-4-ads127/README.md
index,examples/README.md
elf,cmake-build/pat_nucleo_spi1_ads127|cmake-build/pat_nucleo_spi2_ads127|cmake-build/pat_nucleo_spi3_ads127|cmake-build/pat_nucleo_spi4_ads127|main_single_ads127_spi.c PAT_ADS127_SINGLE_SPI_BUS
flash,-SingleSpi 1|Flash-Stm32CubeOpenOCD.ps1
```

```toon
example_ref[SPI1-4 net scan sequential single-channel phases]
readme,examples/spi1-4-net-scan/README.md
index,examples/README.md
elf,cmake-build/pat_nucleo_spi1_4_scan.elf|main_spi1_4_scan.c
flash,-Spi1_4|-Spi123 alias
```

Overlap: **schematics + `src/` win** over this file and over rules.
