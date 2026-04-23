# `.cursor/skills/` pack layout

| Path | Role |
|------|------|
| **`README.md`** | Index, reading order, skill matrix (not a skill entrypoint). |
| **`Folder_Structure.md`** | This file — L2 pack tree. |
| **`stm32cube-cmake-pat/SKILL.md`** | CMake + Ninja build, OpenOCD flash (`metadata.pattern: pipeline`). |
| **`stm32cube-hal-model/SKILL.md`** | HAL handle / MspInit / State; **`references/reference-um2217.md`**. |
| **`stm32h7-hal-pitfalls/SKILL.md`** | SysTick, `HAL_InitTick`, PLL tick. |
| **`platformio-stm32-pat/SKILL.md`** | Optional PlatformIO path. |
| **`single-channel-spi4-ads127/SKILL.md`** | One ADS127: SPI4 default + SPI1–4 single-bus ELFs (legacy folder name). |
| **`spi2-pc2c-miso-h7-pat/SKILL.md`** | SPI2 PC2 / `PC2SO`, logical ch1. |
| **`four-channel-spi-ads127-quartet/SKILL.md`** | Quartet SPI1→4, `pat_nucleo_quartet`. |

Each skill is **one directory** with **`SKILL.md`** at its root. Optional **`references/`**, **`assets/`**, **`tools/`** per skill when content grows.
