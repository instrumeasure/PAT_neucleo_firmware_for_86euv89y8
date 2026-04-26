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
quartet_order,ch0..3=SPI1..SPI4|`pat_nucleo_quartet`: always **`PAT_QUARTET_PARALLEL_DRDY_WAIT=1`** (shared !CS + DRDY epoch; no per-channel sequential quartet in-tree). **`PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER=ON` (default)** → interleaved register quartet SPI (`pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`, no HAL SPI IT); **OFF** → `HAL_SPI_TransmitReceive_IT` + NVIC; **`PAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY=ON` (default)** → SPI4 MISO (`ctxs[3]`) DRDY gate only; **OFF** → all four MISO; UART milestones `main_quartet.c`; bring-up: one **nRESET** + `ads127_bringup_no_nreset` per bus
quartet_ti_3wire_spi,optional CMake|`PAT_ADS127_SPI_3WIRE_CS_HELD_LOW=ON` (quartet target only): SBAS946 §8.5.9 — all four !CS **held low** from GPIO init through nRESET so each ADS127 latches 3-wire (STATUS.CS_MODE=1); firmware never drives !CS high; frames delimited by SCLK count; see `ads127l11.c` / `main_quartet.c`
quartet_epoch_vs_odr,UART+LA|`EPOCH,...,epoch_hz_est` ≈ 1e6/`span_us` (MCU quartet **epoch** rate); nominal per-die ODR ~`sample_hz` (TIM6 note is `main.c` only); LA on **!CS** (4-wire) ≈ two edges per epoch on each chip-select; **SCLK** is bursty; CMake throughput knobs (quartet only): `PAT_SPI123_PRESCALER_DIV` 8|16|32|64, `PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY` 0–15, `PAT_QUARTET_DIAG_EPOCH_EVERY`, `PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED` (requires parallel ON); future DMA tier-B — [include/pat_quartet_p4_dma.h](include/pat_quartet_p4_dma.h), [examples/four-channel-spi1-4-ads127/README.md](examples/four-channel-spi1-4-ads127/README.md)
clk_source,HAT generates CLK → MCU does not drive unless you add MCO/TIM doc’d in code
qpd_dsp,3×16 ring per path raw|I|Q y=(sum)>>4|p nibble in SPI6 hdr
spi6_j2,PA5 SCK PG8 NSS PG12 MISO PG14 MOSI|SPI6 slave fmt0x02 64B LE sample_index+8B hdr+4×3×24b BE+pad|IT double-buffer
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
nvic,TIM6 DAC vs SPI6 priorities|5 vs 6 typical
it_dma,SPI6 IT+static buffers|DMA only if D-cache rules met
pointers,stm32h7-hal-pitfalls|stm32-firmware.mdc
```

USART3 **PD8/PD9** → ST-Link VCP **115200**.

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
