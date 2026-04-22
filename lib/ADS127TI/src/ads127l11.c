#include "ads127l11.h"
#include "ads127l11_hal_stm32.h"

#define ADS127_CMD_RREG(addr) ((uint8_t)(0x20U | ((addr) & 0x1FU)))
#define ADS127_CMD_WREG(addr) ((uint8_t)(0x40U | ((addr) & 0x1FU)))

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

void ads127_init_device(ads127_device_t *dev, uint8_t channel)
{
    uint32_t i;

    dev->channel = channel;
    dev->configured = false;
    for (i = 0; i < ADS127_REGISTER_MAP_SIZE; i++)
    {
        dev->register_map[i] = 0U;
    }
}

bool ads127_startup(ads127_device_t *dev)
{
    if (dev == 0 || dev->channel >= ADS127_SYNC_CHANNELS)
    {
        return false;
    }

    ADS127_HAL_DelayMs(50U);
    ADS127_HAL_SetRESET(true);
    ADS127_HAL_SetSTART(true);
    ADS127_HAL_ToggleRESET(2U);
    ADS127_HAL_DelayMs(2U);

    /*
     * CONFIG2 default 00h: high-speed mode (25.6 MHz class), START/stop, powers OK.
     * Writes to CONFIG3/4 restart conversion path per SBAS946 — order per TI flow.
     */
    if (!ads127_write_single_register(dev, ADS127_REG_CONFIG2, 0x00U))
    {
        return false;
    }

    if (!ads127_write_single_register(dev, ADS127_REG_CONFIG3, ADS127_CONFIG3_WIDEBAND_OSR256))
    {
        return false;
    }

    if (!ads127_write_single_register(dev, ADS127_REG_CONFIG4, ADS127_CONFIG4_USER))
    {
        return false;
    }

    dev->configured = true;
    return true;
}

bool ads127_read_data(ads127_device_t *dev, int32_t *raw24)
{
    uint8_t tx[3] = {0, 0, 0};
    uint8_t rx[3] = {0, 0, 0};
    HAL_StatusTypeDef st;
    int32_t value;

    if (dev == 0 || raw24 == 0 || !dev->configured)
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
