#include "qpd_spi6_slave.h"
#include <string.h>

#define QPD_SPI6_FRAME_N 64U
#define QPD_SPI6_FMT_0x02 0x02U

static SPI_HandleTypeDef *g_hspi6;
static uint8_t g_tx_onwire[64];
static uint8_t g_tx_staging[64];
static uint8_t g_rx_discard[64];
static volatile uint8_t g_staging_valid;

static void put_i24_be(uint8_t *p, int32_t v)
{
    int32_t c = v;
    if (c > 8388607L)
    {
        c = 8388607L;
    }
    if (c < -8388608L)
    {
        c = -8388608L;
    }
    p[0] = (uint8_t)(((uint32_t)(c) >> 16) & 0xFFU);
    p[1] = (uint8_t)(((uint32_t)(c) >> 8) & 0xFFU);
    p[2] = (uint8_t)((uint32_t)(c) & 0xFFU);
}

void qpd_spi6_slave_init(SPI_HandleTypeDef *hspi)
{
    g_hspi6 = hspi;
    memset(g_tx_onwire, 0, (size_t)QPD_SPI6_FRAME_N);
    memset(g_tx_staging, 0, (size_t)QPD_SPI6_FRAME_N);
    memset(g_rx_discard, 0, (size_t)QPD_SPI6_FRAME_N);
    g_staging_valid = 0U;

    if (g_hspi6 != NULL)
    {
        (void)HAL_SPI_TransmitReceive_IT(g_hspi6, g_tx_onwire, g_rx_discard, (uint16_t)QPD_SPI6_FRAME_N);
    }
}

void qpd_spi6_slave_pack_latest(const ads127_sample_set_t *s, const qpd_dsp_output_t *dsp)
{
    uint8_t f[64];
    uint8_t ch;
    uint8_t *q;
    uint32_t i;

    if (s == NULL || dsp == NULL)
    {
        return;
    }

    f[0] = (uint8_t)(s->sample_index & 0xFFU);
    f[1] = (uint8_t)((s->sample_index >> 8) & 0xFFU);
    f[2] = (uint8_t)((s->sample_index >> 16) & 0xFFU);
    f[3] = (uint8_t)((s->sample_index >> 24) & 0xFFU);
    f[4] = QPD_SPI6_FMT_0x02;
    f[5] = dsp->p_lo & 0x0FU;
    f[6] = 0U;
    f[7] = 0U;

    q = &f[8];
    for (ch = 0U; ch < ADS127_SYNC_CHANNELS; ch++)
    {
        put_i24_be(q, dsp->y_raw[ch]);
        q += 3U;
        put_i24_be(q, dsp->y_i[ch]);
        q += 3U;
        put_i24_be(q, dsp->y_q[ch]);
        q += 3U;
    }
    for (i = 44U; i < 64U; i++)
    {
        f[i] = 0U;
    }

    {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        memcpy(g_tx_staging, f, 64U);
        g_staging_valid = 1U;
        __set_PRIMASK(primask);
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (g_hspi6 == NULL || hspi != g_hspi6)
    {
        return;
    }

    if (g_staging_valid != 0U)
    {
        memcpy(g_tx_onwire, g_tx_staging, 64U);
        g_staging_valid = 0U;
    }

    (void)HAL_SPI_TransmitReceive_IT(g_hspi6, g_tx_onwire, g_rx_discard, (uint16_t)QPD_SPI6_FRAME_N);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (g_hspi6 == NULL || hspi != g_hspi6)
    {
        return;
    }
    (void)HAL_SPI_TransmitReceive_IT(g_hspi6, g_tx_onwire, g_rx_discard, (uint16_t)QPD_SPI6_FRAME_N);
}
