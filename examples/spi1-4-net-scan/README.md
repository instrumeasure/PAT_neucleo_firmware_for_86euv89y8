# Example codebase: SPI1–4 net scan (one bus at a time)

Sequential **bring-up → post-START gate → stream** on **SPI1**, then **SPI2**, **SPI3**, **SPI4**, then repeat. Same **single-channel** behaviour as **`pat_nucleo_h753`** / **`main_single_ads127_spi.c`**, but **`src/main_spi1_4_scan.c`** re-inits one **`SPI_HandleTypeDef`** per phase and only the active bus’s pins are used for traffic.

## Firmware

| Path | Role |
|------|------|
| `src/main_spi1_4_scan.c` | Phase table, `run_one_phase`, `SPI_BUS_SCAN_PHASE_MS` dwell per bus, `HAL_SPI_DeInit` between phases |
| `src/ads127l11.c` | Shared with single-bus / default apps |

**Phase budget:** **`SPI_BUS_SCAN_PHASE_MS`** in `main_spi1_4_scan.c` (default **3000 ms**) is a **wall-clock cap from `HAL_SPI_Init` for that bus** through bring-up, post-START gate, and streaming combined; UART prints elapsed time at end of each phase.

## Build and flash

```powershell
powershell -ExecutionPolicy Bypass -File scripts/Build-Stm32CubeCMake.ps1
powershell -ExecutionPolicy Bypass -File scripts/Flash-Stm32CubeOpenOCD.ps1 -Spi1_4
```

Alias: **`-Spi123`**. ELF: **`cmake-build/pat_nucleo_spi1_4_scan.elf`**.

## Related

- Fixed-bus examples: [single-bus-spi1-4-ads127](../single-bus-spi1-4-ads127/README.md)
- Quartet: [four-channel-spi1-4-ads127](../four-channel-spi1-4-ads127/README.md)
