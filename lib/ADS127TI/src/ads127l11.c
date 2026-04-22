#include "ads127l11.h"
#include "ads127l11_hal_stm32.h"

/* SBAS946 Table 8-14: read register 40h + addr[3:0]; write register 80h + addr[3:0]. */
#define ADS127_CMD_RREG(addr) ((uint8_t)(0x40U | ((uint8_t)(addr) & 0x0FU)))
#define ADS127_CMD_WREG(addr) ((uint8_t)(0x80U | ((uint8_t)(addr) & 0x0FU)))

/*
 * SBAS946 digital timing (see switching characteristics, SPI + RESET):
 * - tw(RSL): nRESET low ≥ 4·tCLK (master clock at CLK pin, not SCLK).
 * - td(RSSC): wait ≥ 10000·tCLK after nRESET rising before first SPI / conversion clock.
 *   Example: f_CLK = 2.5 MHz → t_CLK = 400 ns → need ≥ 4 ms; 25 MHz → ≥ 0.4 ms.
 * - td(CSSC): first SCLK rising ≥ 10 ns after CS falling (met by normal GPIO+SPI latency).
 * - td(SCCS): CS rising ≥ 10 ns after last SCLK falling; tw(CSH): CS high ≥ 20 ns.
 */
#ifndef ADS127_RESET_PULSE_MS
#define ADS127_RESET_PULSE_MS 2U
#endif
#ifndef ADS127_POST_RESET_SPI_SETTLE_MS
#define ADS127_POST_RESET_SPI_SETTLE_MS 10U
#endif

static uint32_t g_sample_counter = 0U;

static bool ads127_write_single_register(ads127_device_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t tx[2];
    uint8_t rx[2];
    HAL_StatusTypeDef st;

    tx[0] = ADS127_CMD_WREG(reg);
    tx[1] = value;
    st = ADS127_HAL_SPI_Transfer(dev->channel, tx, rx, 2U, 10U);
    if (st == HAL_OK)
    {
        dev->register_map[reg] = value;
        return true;
    }

    return false;
}

/* Mirror TI clearSTATUSflags(): SBAS946 STATUS — write-1-to-clear ALV/POR/SPI_ERR (see TI ads127l11.c). */
static bool ads127_clear_status_errors(ads127_device_t *dev)
{
    if (!ads127_write_single_register(dev, ADS127_REG_STATUS, ADS127_STATUS_CLEAR_ERRORS))
    {
        return false;
    }

    dev->register_map[ADS127_REG_STATUS] = 0U;
    return true;
}

bool ads127_read_register(ads127_device_t *dev, uint8_t reg, uint8_t *value, uint32_t *hal_cmd_out, uint32_t *hal_nop_out)
{
    uint8_t tx1[2];
    uint8_t rx1[2];
    uint8_t tx2[2];
    uint8_t rx2[2];
    HAL_StatusTypeDef st;

    if (dev == 0 || value == 0 || dev->channel >= ADS127_SYNC_CHANNELS)
    {
        return false;
    }

    tx1[0] = ADS127_CMD_RREG(reg);
    tx1[1] = 0x00U;
    st = ADS127_HAL_SPI_Transfer(dev->channel, tx1, rx1, 2U, 10U);
    if (hal_cmd_out != 0)
    {
        *hal_cmd_out = (uint32_t)st;
    }
    if (st != HAL_OK)
    {
        return false;
    }

    tx2[0] = 0x00U;
    tx2[1] = 0x00U;
    st = ADS127_HAL_SPI_Transfer(dev->channel, tx2, rx2, 2U, 10U);
    if (hal_nop_out != 0)
    {
        *hal_nop_out = (uint32_t)st;
    }
    if (st != HAL_OK)
    {
        return false;
    }

    *value = rx2[0];
    (void)rx1[0];
    (void)rx1[1];
    return true;
}

bool ads127_spi_poll_dev_rev(ads127_device_t *dev, uint32_t *hal_dev_cmd_out, uint32_t *hal_dev_nop_out)
{
    if (dev == 0 || dev->channel >= ADS127_SYNC_CHANNELS)
    {
        return false;
    }

    if (!ads127_read_register(dev, ADS127_REG_DEV_ID, &dev->dev_id_hw, hal_dev_cmd_out, hal_dev_nop_out))
    {
        return false;
    }

    if (!ads127_read_register(dev, ADS127_REG_REV_ID, &dev->rev_id_hw, 0, 0))
    {
        return false;
    }

    return dev->dev_id_hw == ADS127_DEV_ID_EXPECTED;
}

/* SBAS946 §8.5: conversion data — assert !CS, clock 24 bits; MOSI hold low (no command). */
static bool ads127_spi_read_conversion(ads127_device_t *dev, int32_t *raw24)
{
    uint8_t tx[3] = {0, 0, 0};
    uint8_t rx[3] = {0, 0, 0};
    HAL_StatusTypeDef st;
    int32_t value;

    if (dev == 0 || raw24 == 0)
    {
        return false;
    }

    st = ADS127_HAL_SPI_Transfer(dev->channel, tx, rx, 3U, 10U);
    if (st != HAL_OK)
    {
        return false;
    }

    value = ((int32_t)rx[0] << 16) | ((int32_t)rx[1] << 8) | rx[2];
    if ((value & 0x800000L) != 0)
    {
        value |= 0xFF000000L;
    }

    *raw24 = value;
    return true;
}

void ads127_init_device(ads127_device_t *dev, uint8_t channel)
{
    uint32_t i;

    dev->channel = channel;
    dev->configured = false;
    dev->dev_id_hw = 0xFFU;
    dev->rev_id_hw = 0xFFU;
    dev->status_hw = 0xFFU;
    for (i = 0; i < ADS127_REGISTER_MAP_SIZE; i++)
    {
        dev->register_map[i] = 0U;
    }
}

bool ads127_spi_verify_link(ads127_device_t *dev)
{
    if (dev == 0 || dev->channel >= ADS127_SYNC_CHANNELS)
    {
        return false;
    }

    ADS127_HAL_DelayMs(50U);
    ADS127_HAL_SetRESET(true);
    ADS127_HAL_SetSTART(true);
    ADS127_HAL_ToggleRESET(ADS127_RESET_PULSE_MS);
    /* td(RSSC): do not clock CS/SPI until 10000·tCLK after nRESET goes high (SBAS946). */
    ADS127_HAL_DelayMs(ADS127_POST_RESET_SPI_SETTLE_MS);

    /* Table 8-16: read DEV_ID (0h) first, then REV_ID (1h). */
    if (!ads127_read_register(dev, ADS127_REG_DEV_ID, &dev->dev_id_hw, 0, 0))
    {
        return false;
    }

    if (dev->dev_id_hw != ADS127_DEV_ID_EXPECTED)
    {
        return false;
    }

    if (!ads127_read_register(dev, ADS127_REG_REV_ID, &dev->rev_id_hw, 0, 0))
    {
        return false;
    }

    dev->configured = false;
    return true;
}

bool ads127_startup(ads127_device_t *dev)
{
    if (dev == 0 || dev->channel >= ADS127_SYNC_CHANNELS)
    {
        return false;
    }

    if (!ads127_spi_verify_link(dev))
    {
        return false;
    }

    /*
     * POR-default mode (SBAS946 / TI restoreRegisterDefaults): no CONFIG2/3/4 WREG.
     * Clear STATUS error flags only so later optional WREG (or a future profile) is not blocked.
     */
    if (!ads127_clear_status_errors(dev))
    {
        return false;
    }

    dev->configured = true;
    return true;
}

bool ads127_read_data(ads127_device_t *dev, int32_t *raw24)
{
    if (dev == 0 || raw24 == 0 || !dev->configured)
    {
        return false;
    }

    return ads127_spi_read_conversion(dev, raw24);
}

bool ads127_read_synchronous_quartet(ads127_device_t devs[ADS127_SYNC_CHANNELS], ads127_sample_set_t *set)
{
    uint32_t i;
    int32_t tmp_raw[ADS127_SYNC_CHANNELS];

    if (set == 0)
    {
        return false;
    }

    for (i = 0; i < ADS127_SYNC_CHANNELS; i++)
    {
        if (!ads127_read_data(&devs[i], &tmp_raw[i]))
        {
            return false;
        }
    }

    for (i = 0; i < ADS127_SYNC_CHANNELS; i++)
    {
        set->raw[i] = tmp_raw[i];
    }
    g_sample_counter++;
    set->sample_index = g_sample_counter;
    return true;
}

uint32_t ads127_get_quartet_acquired_count(void)
{
    return g_sample_counter;
}
