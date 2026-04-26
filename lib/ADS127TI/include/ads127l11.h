#ifndef ADS127L11_H
#define ADS127L11_H

#include <stdbool.h>
#include <stdint.h>

#define ADS127_REGISTER_MAP_SIZE 16U
#define ADS127_SYNC_CHANNELS 4U
#define ADS127_RAW_INVALID ((int32_t)-1)

/* Register map (SBAS946, 8-bit addresses) */
#define ADS127_REG_DEV_ID 0x00U
#define ADS127_REG_REV_ID 0x01U
#define ADS127_REG_STATUS 0x02U
/* TI precision-adc-examples: write 1 to clear ALV_FLAG, POR_FLAG, SPI_ERR (unblocks WREG if SPI_ERR latched). */
#define ADS127_STATUS_CLEAR_ERRORS 0x70U
#define ADS127_REG_CONTROL 0x03U
#define ADS127_REG_MUX 0x04U
#define ADS127_REG_CONFIG1 0x05U
#define ADS127_REG_CONFIG2 0x06U
#define ADS127_REG_CONFIG3 0x07U
#define ADS127_REG_CONFIG4 0x08U

/*
 * CONFIG3 FILTER[4:0] (Table 8-25): 00100b = wideband, OSR 512.
 * (00011b was OSR256 — superseded in PAT bring-up.)
 */
#define ADS127_CONFIG3_WIDEBAND_OSR512 0x04U

/*
 * CONFIG4 bit7: 0x80 = external master clock on CLK; 0x00 = internal 25.6 MHz (SBAS946 POR default).
 * Startup leaves CONFIG registers at POR — this macro only gates TIM6 SAMPLE_RATE_HZ in main.c.
 * For legacy HAT with 25 MHz on CLK and CONFIG4 written to 0x80, pass -DADS127_CONFIG4_USER=0x80.
 */
#ifndef ADS127_CONFIG4_USER
#define ADS127_CONFIG4_USER 0x00U
#endif

/* Match TIM6 / quartet polling to OSR512 wideband ODR vs modulator f_CLK (SBAS946 Table 7-1). */
#define ADS127_ODR_HZ_NOMINAL_25M6_MHZ 25000U
/* ~24.4 kSPS @ 25.0 MHz FCLK, wideband OSR512 (half of former OSR256 ODR). */
#define ADS127_ODR_HZ_25M_EXT 24414U

/* SBAS946 Table 8-18: DEV_ID 00h identifies ADS127L11. */
#define ADS127_DEV_ID_EXPECTED 0x00U

typedef struct
{
    uint8_t channel;
    uint8_t register_map[ADS127_REGISTER_MAP_SIZE];
    bool configured;
    /* Last values from SPI register read (startup probe); 0xFF if never read. */
    uint8_t dev_id_hw;
    uint8_t rev_id_hw;
    uint8_t status_hw;
} ads127_device_t;

typedef struct
{
    int32_t raw[ADS127_SYNC_CHANNELS];
    uint32_t sample_index;
} ads127_sample_set_t;

void ads127_init_device(ads127_device_t *dev, uint8_t channel);
/* SBAS946 Figure 8-29: RREG off-frame (command frame, then NOP frame); *value from second byte of NOP frame. */
bool ads127_read_register(ads127_device_t *dev, uint8_t reg, uint8_t *value, uint32_t *hal_cmd_out, uint32_t *hal_nop_out);
/* RREG DEV_ID then REV_ID only (no nRESET). Optional HAL status from DEV_ID RREG for bring-up. */
bool ads127_spi_poll_dev_rev(ads127_device_t *dev, uint32_t *hal_dev_cmd_out, uint32_t *hal_dev_nop_out);
/* nRESET/START + td(RSSC) wait, then RREG DEV_ID then REV_ID only (SBAS946 order). */
bool ads127_spi_verify_link(ads127_device_t *dev);
bool ads127_startup(ads127_device_t *dev);
bool ads127_read_data(ads127_device_t *dev, int32_t *raw24);
bool ads127_read_synchronous_quartet(ads127_device_t devs[ADS127_SYNC_CHANNELS], ads127_sample_set_t *set);

/* Successful synchronous quartet reads since boot (four channels per count). */
uint32_t ads127_get_quartet_acquired_count(void);

#endif
