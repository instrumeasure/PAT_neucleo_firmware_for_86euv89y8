#include "pat_crc32.h"

uint32_t pat_crc32_ieee(const uint8_t *data, size_t n)
{
  uint32_t crc = 0xFFFFFFFFu;
  size_t i;
  uint8_t b;
  uint8_t bit;
  if (data == 0) {
    return 0u;
  }
  for (i = 0; i < n; i++) {
    b = data[i];
    crc ^= b;
    for (bit = 0u; bit < 8u; bit++) {
      if ((crc & 1u) != 0u) {
        crc = (crc >> 1) ^ 0xEDB88320u;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}
