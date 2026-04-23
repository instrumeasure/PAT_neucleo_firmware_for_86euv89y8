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
flash_openocd,powershell -File scripts/Flash-Stm32CubeOpenOCD.ps1
hal_pack,STM32_CUBE_H7_FW or %USERPROFILE%\.platformio\packages\framework-stm32cubeh7 (populate via py -m platformio run -e nucleo_h753zi once)
out,cmake-build/pat_nucleo_h753.{elf,bin}
needs,CMake Ninja arm-none-eabi-gcc | skill,.cursor/skills/stm32cube-cmake-pat/SKILL.md
```

Optional: `platformio.ini` / `.cursor/skills/platformio-stm32-pat/SKILL.md` • Cube snapshot `cube/legacy_86euv89y8_h753zi.ioc` • timing/SPI changes → LA/scope CLK CS SCLK DRDY.

```toon
runtime[key]
sample_hz,48828|SAMPLE_RATE_HZ TIM6 when ADS127_CONFIG4_USER=0x80 (25 MHz FCLK) ≈ 50k×25/25.6 OSR256 wideband
sample_hz_alt,50000|if CONFIG4 internal (0x00) @ 25.6 MHz nominal — use ADS127_ODR_HZ_NOMINAL_25M6_MHZ
ext_clk,ADS127_CONFIG4_USER=0x80 default in ads127l11.h (external master clock on CLK per SBAS946)
quartet_order,SPI1→SPI2→SPI3→SPI4|last_raw[4] only if all four reads OK
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
quartets_ok,successful quartet batches since boot ads127_get_quartet_acquired_count()
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
```

Overlap: **schematics + `src/` win** over this file and over rules.
