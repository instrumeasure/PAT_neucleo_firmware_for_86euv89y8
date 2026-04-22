#ifndef ADS127L11_H
#define ADS127L11_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define ADS127_REG_DEV_ID   0x00u
#define ADS127_REG_REV_ID   0x01u
#define ADS127_REG_STATUS   0x02u
#define ADS127_REG_CONTROL  0x03u
#define ADS127_REG_MUX      0x04u
#define ADS127_REG_CONFIG1  0x05u
#define ADS127_REG_CONFIG2  0x06u
#define ADS127_REG_CONFIG3  0x07u
#define ADS127_REG_CONFIG4  0x08u

#define ADS127_CMD_RREG(addr)   ((uint8_t)(0x40u | ((addr) & 0x0Fu)))
#define ADS127_CMD_WREG(addr)   ((uint8_t)(0x80u | ((addr) & 0x0Fu)))

/** Shadow 00h–08h (SBAS946 Table 8-16 subset). */
typedef struct {
  uint8_t dev_id;
  uint8_t rev_id;
  uint8_t status;
  uint8_t control;
  uint8_t mux;
  uint8_t config1;
  uint8_t config2;
  uint8_t config3;
  uint8_t config4;
} ads127_shadow_t;

typedef struct {
  uint32_t fault_mask;
  uint32_t drdy_timeouts;
  uint32_t last_sample_u32_be;
} ads127_diag_t;

void ads127_pins_init(void);
void ads127_nreset_pulse(void);
void ads127_start_set(int run);

HAL_StatusTypeDef ads127_rreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t *out);
HAL_StatusTypeDef ads127_wreg(SPI_HandleTypeDef *hspi, uint8_t addr, uint8_t data);

HAL_StatusTypeDef ads127_shadow_refresh(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh);

/**
 * Milestone 1 bring-up: external CLK (CONFIG4 bit7), SDO_MODE+START_MODE in CONFIG2,
 * wideband OSR256 CONFIG3=0x03. PF1 held low during WREG to 04h–0Eh.
 */
int ads127_bringup(SPI_HandleTypeDef *hspi, ads127_shadow_t *sh, ads127_diag_t *dg);

/**
 * Phase A: !CS low, poll PE13 (no SCLK) until DRDY indicates ready or timeout.
 * Phase B: 24-bit read (MOSI 0x00,0x00,0x00).
 */
HAL_StatusTypeDef ads127_read_sample24_blocking(
    SPI_HandleTypeDef *hspi,
    uint8_t out24[3],
    uint32_t timeout_ms,
    ads127_diag_t *dg);

#endif
