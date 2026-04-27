# UART7 — laser driver (SF8xxx TO56B class)

**MCU pins (serial):** **PE8** TX → laser, **PE7** RX ← laser (AF7 **UART7**).  
**MCU pins (digital control connector):** **PB8** = `laser_driver_oc` (connector pin 7), **PB9** = `int_lock` (connector pin 6). See [PINMAP.md](../PINMAP.md) § UART7.

**Vendor manual (primary source for wire payload):**  
[Laser Diode Control — SF8XXX_TO56B manual (PDF)](https://www.laserdiodecontrol.com/files/manuals/laserdiodecontrol_com/10237/SF8XXX_TO56B_Manual-1720730740.pdf)

This repo **does not** yet copy the manual’s command tables into firmware. Treat the PDF as **authoritative** for:

- **Baud rate**, parity, stop bits, and any **line-idle** / **termination** rules  
- **Command / response framing** (ASCII text, binary registers, CRC, checksum — **whatever the module defines**)  
- **Timing** between TX and RX (half-duplex turnaround, minimum gaps)  
- **Safe defaults** (enable sequencing, interlocks)

Current implementation status: **`pat_nucleo_mems_bringup`** now wires **UART7** with **`HAL_UARTEx_ReceiveToIdle_DMA`** and a light cache worker (`src/pat_uart7_laser.c`), but the full vendor grammar still needs final frozen command tables from the PDF.

## Payload analysis (firmware planning)

Until the tables are transcribed here:

| Topic | Status |
|-------|--------|
| **Exact byte sequences** | **Extract from PDF** (or vendor CSV / reference host) and add a frozen “wire table” subsection below when agreed. |
| **MCU role** | **UART host** — firmware issues driver commands on **UART7**; the laser module is the **peripheral**. |
| **Relation to UART5** | PolarFire speaks **PAT5** on **UART5** only. **Laser-native** bytes must **not** be sent on UART5 raw; they travel inside a **bypass** PAT5 command (see [UART5_POLARFIRE_PAYLOAD.md §8](UART5_POLARFIRE_PAYLOAD.md)). |
| **Baud mismatch** | **UART5** link speed is fixed by the PolarFire bridge doc (**921600** 8N1 v1). **UART7** speed is whatever the **SF8xxx** manual requires — the MCU **re-frames** at the UART7 line rate (no wire-level join between PF and laser). |
| **Status to PolarFire** | Firmware runs a **low-rate** **UART7** **status** read (vendor command from PDF) in **main** or on a **slow tick**, keeps **`pat_laser_status_cache`** in RAM. PolarFire reads it with **`GET_LASER_STATUS` (`0x0005`)** on **UART5** — **copy from cache**, no per-request **UART7** traffic in v1 ([UART5 §9](UART5_POLARFIRE_PAYLOAD.md)). |
| **RX / TX implementation** | Recommended: **DMA** + **UART IDLE** (or **`HAL_UARTEx_ReceiveToIdle_DMA`**) for **RX** into a circular or double buffer; **ISR stays thin** (byte count / half-transfer flag only). **Vendor parser** runs in **main** — same *shape* as UART5 **§2.3** (transport bytes → complete **laser** frames) but **not** PAT5 — see **§ DMA + vendor parser** below. |

## Digital control GPIO (PB8 / PB9)

These two control lines are separate from UART serial framing:

| Signal | MCU pin | Connector pin | Default planning note |
|-------|---------|---------------|------------------------|
| `laser_driver_oc` | **PB8** | **7** | Treat as **fault/OC** net. Until schematic freeze, initialise as safe GPIO (typically input with pull policy from board). |
| `int_lock` | **PB9** | **6** | Treat as **interlock/lock** control net. Do not drive active level until polarity + direction are frozen. |

**Do not guess active levels in code:** final **direction** (MCU input/output), **polarity**, and sequencing with **UART7** commands come from the hardware schematic + vendor integration notes. Track pin ownership in [PINMAP.md](../PINMAP.md).

## DMA + vendor parser (UART7 wire — not PAT5)

**UART7** does **not** carry **`PAT5`** frames. The **parser** here means: turn the **RX byte stream** into **complete laser messages** per the **SF8xxx** PDF (ASCII line terminated by CR/LF, fixed-length binary blocks, checksum/CRC — **whatever the manual defines**). Until that grammar is frozen, treat the parser as a **pluggable** module (e.g. `pat_uart7_vendor_parse.c`) with **tests** from captured traces.

### Recommended HAL shape (STM32H7)

| Layer | Role |
|-------|------|
| **DMA RX** | Circular buffer (or **double buffer**) for **PE7** RX; **UART** **IDLE** line detection and/or **RTO** to bound frames; optional **DMA TX** for long **`0x0004`** opaque bursts. |
| **UART IRQ** | **Minimal**: **`HAL_UARTEx_RxEventCallback`** / IDLE handler — update **write index** or **message length**, **no** string parsing. **ORE** clear + counter if the line runs faster than **main** consumes. |
| **Vendor parser (main)** | Consumes committed RX ranges; outputs **one logical laser response** for **`pat_laser_status_cache`** or for the **`0x0004`** bypass response assembler. |

### D-cache (H7)

If **DMA** writes RX buffers in **AXI SRAM** visible to the **M7 D-cache**, follow the same rules as quartet DMA notes: **non-cacheable MPU region**, **DTCM**, or **`SCB_InvalidateDCache_by_Addr`** around the **exact** committed range before the CPU reads it — see [`include/pat_quartet_p4_dma.h`](../include/pat_quartet_p4_dma.h).

### NVIC

**UART7** and **DMA stream** IRQs for **USART7_RX** sit in the **same loose tier** as **UART5** — **below SPI1–4 and SPI6** ([UART5 doc §7.1](UART5_POLARFIRE_PAYLOAD.md)). **Do not** run the **vendor parser** or **`HAL_UART`** blocking calls from **SPI1–4 / SPI6** ISRs.

## Document history

| Rev | Date | Change |
|-----|------|--------|
| 1 | 2026-04-26 | Stub: PDF link, planning notes, UART5 bypass cross-reference. |
| 2 | 2026-04-26 | Cached status path: background UART7 poll, PolarFire `0x0005` / UART5 §9. |
| 3 | 2026-04-26 | DMA + IDLE RX, vendor parser in main, D-cache note; NVIC folded under DMA section. |
| 4 | 2026-04-27 | Add digital control GPIO PB8 `laser_driver_oc` / PB9 `int_lock` with connector pins and safe-default guidance. |
| 5 | 2026-04-27 | Mark UART7 DMA+IDLE implementation in `pat_nucleo_mems_bringup` and fix AF naming (`UART7`). |
