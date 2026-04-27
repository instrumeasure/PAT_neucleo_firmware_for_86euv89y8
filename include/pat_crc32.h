#ifndef PAT_CRC32_H
#define PAT_CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t pat_crc32_ieee(const uint8_t *data, size_t n);

#endif
