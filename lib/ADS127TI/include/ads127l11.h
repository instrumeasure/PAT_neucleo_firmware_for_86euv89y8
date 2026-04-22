#ifndef ADS127L11_H
#define ADS127L11_H

#include <stdbool.h>
#include <stdint.h>

#define ADS127_REGISTER_MAP_SIZE 16U
#define ADS127_SYNC_CHANNELS 4U
#define ADS127_RAW_INVALID ((int32_t)-1)

/* Register map (SBAS946, 8-bit addresses) */
#define ADS127_REG_DEV_ID 0x00U
#define ADS127_REG_STATUS 0x02U
#define ADS127_REG_CONTROL 0x03U
#define ADS127_REG_MUX 0x04U
#define ADS127_REG_CONFIG1 0x05U
#define ADS127_REG_CONFIG2 0x06U
#define ADS127_REG_CONFIG3 0x07U
#define ADS127_REG_CONFIG4 0x08U

/*
 * CONFIG3 FILTER[4:0] (Table 8-25): 00011b = wideband, OSR 256.
 * High-speed mode: ODR = 50 kSPS @ f_CLK = 25.6 MHz (SBAS946 Table 7-1).
 */
#define ADS127_CONFIG3_WIDEBAND_OSR256 0x03U

/*
 * CONFIG4: set bit7 (0x80) for external master clock on CLK pin; 0x00 uses internal 25.6 MHz.
 * Board with a crystal to ADS127 CLK should use external.
 */
#ifndef ADS127_CONFIG4_USER
/* Default: external clock on CLK pin (board 25 MHz); matches startup CONFIG4 write (SBAS946). */
#define ADS127_CONFIG4_USER 0x80U
#endif

/* Match TIM6 / quartet polling to OSR256 wideband ODR vs modulator f_CLK (SBAS946 Table 7-1). */
#define ADS127_ODR_HZ_NOMINAL_25M6_MHZ 50000U
/* ~48.8 kSPS @ 25.0 MHz FCLK (scale from 50 k @ 25.6 MHz). */
#define ADS127_ODR_HZ_25M_EXT 48828U

typedef struct
{
    uint8_t channel;
    uint8_t register_map[ADS127_REGISTER_MAP_SIZE];
    bool configured;
} ads127_device_t;

typedef struct
{
    int32_t raw[ADS127_SYNC_CHANNELS];
    uint32_t sample_index;
} ads127_sample_set_t;

void ads127_init_device(ads127_device_t *dev, uint8_t channel);
bool ads127_startup(ads127_device_t *dev);
bool ads127_read_data(ads127_device_t *dev, int32_t *raw24);
bool ads127_read_synchronous_quartet(ads127_device_t devs[ADS127_SYNC_CHANNELS], ads127_sample_set_t *set);

/* Successful synchronous quartet reads since boot (four channels per count). */
uint32_t ads127_get_quartet_acquired_count(void);

#endif
