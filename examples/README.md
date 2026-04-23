# Examples

| Example | Description |
|---------|-------------|
| [single-channel-spi4-ads127](single-channel-spi4-ads127/README.md) | Default `pat_nucleo_h753` app: SPI4 + one ADS127L11, USART3 log (`main.c`) |
| [single-bus-spi1-4-ads127](single-bus-spi1-4-ads127/README.md) | **Example codebase** for `pat_nucleo_spi1_ads127` … `pat_nucleo_spi4_ads127`: one ADS127 per ELF, `main_single_ads127_spi.c`; flash `-SingleSpi 1` … `4` |
| [spi1-4-net-scan](spi1-4-net-scan/README.md) | `pat_nucleo_spi1_4_scan`: same workflow as single-channel, **one SPI at a time** SPI1 → SPI2 → SPI3 → SPI4 (repeat); flash `-Spi1_4` or `-Spi123` |
| [four-channel-spi1-4-ads127](four-channel-spi1-4-ads127/README.md) | `pat_nucleo_quartet`: SPI1–4 + four ADS127L11, scan-synchronous epoch SPI1→SPI4 |
