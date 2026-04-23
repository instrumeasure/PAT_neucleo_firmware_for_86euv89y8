/**
 * @file spi6_test_frame.h
 * @brief 64-byte SPI6 slave test payload for bench / host verification (fmt tag 0xA5, not QPD 0x02).
 *
 * Layout (little-endian where noted):
 * - Bytes 0–3: uint32_t sequence (LE). Use spi6_test_frame_fill(..., SPI6_TEST_FRAME_INDEX_IDLE) before
 *   the first master burst to emit 0xFFFFFFFF here as a sentinel.
 * - Byte 4: format tag 0xA5 (test).
 * - Bytes 5–7: first three MOSI bytes from the master for the completed transfer (from `rx`).
 * - Bytes 8–63: (uint8_t)((seq ^ (i * 0x1Du)) with seq = completed_frame_index + 1).
 */
#ifndef SPI6_TEST_FRAME_H
#define SPI6_TEST_FRAME_H

#include <stdint.h>

#define SPI6_TEST_FRAME_N 64U
/** Pass as `completed_frame_index` for pre-first-transfer idle TX pattern. */
#define SPI6_TEST_FRAME_INDEX_IDLE (0xFFFFFFFFu)

void spi6_test_frame_fill(uint8_t tx[SPI6_TEST_FRAME_N], const uint8_t *rx, uint32_t completed_frame_index);

#endif
