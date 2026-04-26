# PAT Nucleo (86euv89y8) — user manual

This document is the **end-user and integrator** guide for the PAT Nucleo + HAT. Sections will be added (hardware, flashing, J1/J2, build targets) over time. The chapter below is **current** and applies to the **quartet rolling** export and **J2 / SPI6** 64 B host link.

**Related repo references:** [PINMAP.md](../PINMAP.md), [AGENTS.md](../AGENTS.md), [four-channel example README](../examples/four-channel-spi1-4-ads127/README.md).

---

## Instrumentation and data acquisition (quartet rolling export over SPI6)

This section explains what the 64 B frames **mean** for a host, lab script, or dashboard. It is **not** a metrology certificate for SI units; work in **ADC codes** (24-bit, sign-extended to 32 on the host) is sufficient unless your programme adds calibration elsewhere.

**Summary:** The Nucleo publishes **per-channel** rolling **means** of raw, I, and Q paths, plus a header (`epoch_seq`, `fmt`, local DDS `p`, flags). For the rolling quartet image this payload uses **fmt `0x0A`** with `wpos` in header byte 7 and LE `int32` means in bytes 8..55. The host **pulls** each 64 B with SPI as **master** on J2. **Latency and jitter** to the host are **normal and acceptable** on this link—you do not need a fixed time-of-arrival to use the data for trending and algorithm checks.

### What the export is (and is not)

- **`mean_raw` (per channel):** An **8-sample boxcar (moving average)** of sign-extended 24-bit ADC words, with divide-by-8 as a **right shift by 3** in firmware. It is a **linear** smoother in **code** over successive **epoch** samples (not a brick-wall filter). Timing variation in the quartet read and in **when** the host issues a J2 read is **expected** and **not** treated as a product defect in v1.
- **`mean_i` and `mean_q`:** They are **not** classical analogue quadrature products (no multiply in the path). The firmware applies **sign selection** to the same magnitude (±raw) from **look-up tables** that depend on a **3-bit, 8-state** local phase `p` (and optional `step`). Use them as **defined algorithm outputs**; document your LUTs for **reproducibility** if you change them.
- **`p` in the header** is a **firmware** phase counter, not a measured **optical** phase. Log it to **reconstruct** which I/Q sign was used; it is not a sub-degree **phase** instrument.
- **Calibration to real units (e.g. mW):** **Not** required for this product path unless your site adds a separate **calibration** spec.

### Timebase, latency, and `epoch_seq`

*Delay and jitter in when a 64 B frame is received on the host are **largely unavoidable** with asynchronous **pull** on J2, a **cooperative** Nucleo loop, and the quartet on its own cadence. This is **acceptable** for the intended use.*

- **`epoch_seq`:** Increments when a new **full** 64 B payload is **published** (after a good quartet `on_epoch` and fill+flip in firmware). It gives **order** and helps detect **skipped** updates if the host is slower than the Nucleo’s epoch rate. It does **not** embed wall-clock time. You may add a **host** timestamp when a read **completes** for your logs; that is optional, not a firmware field in v1.
- You may **lose** intermediate `epoch_seq` values if the **host** polls J2 **slower** than the quartet; you always see the **latest** published record (see **No DRDY** in [AGENTS.md](../AGENTS.md) for the “pull + sequence” model if documented there).
- On this hardware path, firmware processing after each quartet acquisition is budgeted against a **~20 µs class** `!CS`->`!DRDY` margin. Keep per-epoch work lightweight and avoid synchronous host waits in the acquisition loop.

### Units, saturation, and cold start

- Values are **integer codes** in the same sense as the **ADS127L11** 24-bit, two’s-complement result, sign-extended in firmware.
- If the on-wire pack uses **24 bits per word** in the same style as the QPD build (big-endian 24b on the bus), the firmware may **saturate** to the 24b range. If the pack uses **little-endian `int32`**, the host can see the full sign-extended range; agree the format with the exact image you run.
- **`mean_valid` (or an epoch count ≥ 8),** in `flags` / header, indicates that the 8-tap **mean** has **warmed** through **eight** good samples; do not treat early means like a **settled** reading until that bit is set (DMM-style behaviour).

### Channel and fixture labelling

- In default **quartet** wiring, **logical ch0..3** map to **SPI1..SPI4** on the HAT. The mapping from **front-panel fibre / optics** to **which** logical channel is a **lab** or **as-built** table—**copy** the pinmap and the four-channel [README](../examples/four-channel-spi1-4-ads127/README.md) so “spike on ch2” is traceable.

### What to log in scripts (recommended)

- Each successful 64 B read: `epoch_seq`, `fmt`, `p`, `wpos` (as defined in your image), all 12 **means** (4× raw, 4× I, 4× Q), and optionally one **host** time stamp. Compare successive **`epoch_seq`** to your read rate to spot **dropped** epochs.
- If you A/B test LUTs, fix **`p` step** and tables and change only the **optical** or **electrical** condition. Store `p` so the sign path is **re-verifiable** offline.
- If SPI reports **incomplete** length, **error**, or **timeout**—**discard** that read; do not append to time series as a valid point.
- If you run SPI6 in the quartet image, keep exactly **one** firmware writer for the staged 64 B frame in the epoch loop; do not mix multiple pack paths in the same run.

### Concise reference table

| Concern | Notes |
|--------|--------|
| I/Q in **physics** terms? | **LUT** sign of **raw**; do not assume **analogue** quadrature without a separate model |
| **Latency / jitter** | **Inevitable, acceptable**; not a v1 “must minimise” spec |
| **Time** in the record | `epoch_seq` = **order**; add **host** time stamp if you need a clock |
| **Missed** epochs | OK by design; last published wins; detect via `epoch_seq` **gaps** in logs |
| **SI** calibration | **Not** in scope for this export path in v1 |
| **Cold start** | **Gate** on `mean_valid` (or 8+ epochs) |
| **Smoother vs per-sample** | 8-tap **mean** has **different** noise stats than a single code |

*End of quartet rolling / J2 instrumentation chapter. Wire layout and J2 SCLK rules: [PINMAP.md](../PINMAP.md) and [AGENTS.md](../AGENTS.md) (`spi6_j2`, `spi6_sclk`). Development plan detail may appear in the repo’s `.cursor/plans/`; this file is the **user-facing** home for interpretation of exports.*
