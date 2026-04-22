#include "ads127l11_hal_stm32.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} ads127_gpio_t;

static SPI_HandleTypeDef *g_spi[ADS127_CHANNEL_COUNT] = {0};
static ads127_gpio_t g_cs[ADS127_CHANNEL_COUNT] = {0};
static ads127_gpio_t g_start = {0};
static ads127_gpio_t g_reset = {0};

void ADS127_HAL_BindSpi(uint8_t channel, SPI_HandleTypeDef *hspi)
{
    if (channel < ADS127_CHANNEL_COUNT)
    {
        g_spi[channel] = hspi;
    }
}

void ADS127_HAL_SetCsPin(uint8_t channel, GPIO_TypeDef *port, uint16_t pin)
{
    if (channel < ADS127_CHANNEL_COUNT)
    {
        g_cs[channel].port = port;
        g_cs[channel].pin = pin;
    }
}

void ADS127_HAL_SetStartPin(GPIO_TypeDef *port, uint16_t pin)
{
    g_start.port = port;
    g_start.pin = pin;
}

void ADS127_HAL_SetResetPin(GPIO_TypeDef *port, uint16_t pin)
{
    g_reset.port = port;
    g_reset.pin = pin;
}

HAL_StatusTypeDef ADS127_HAL_SPI_Transfer(uint8_t channel, const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;

    if (channel >= ADS127_CHANNEL_COUNT || g_spi[channel] == 0 || len == 0U)
    {
        return HAL_ERROR;
    }

    ADS127_HAL_SetCS(channel, true);
    status = HAL_SPI_TransmitReceive(g_spi[channel], (uint8_t *)tx, rx, len, timeout_ms);
    ADS127_HAL_SetCS(channel, false);

    return status;
}

void ADS127_HAL_SetCS(uint8_t channel, bool asserted)
{
    GPIO_PinState state;

    if (channel >= ADS127_CHANNEL_COUNT || g_cs[channel].port == 0)
    {
        return;
    }

    state = asserted ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(g_cs[channel].port, g_cs[channel].pin, state);
}

void ADS127_HAL_SetSTART(bool high)
{
    if (g_start.port != 0)
    {
        HAL_GPIO_WritePin(g_start.port, g_start.pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void ADS127_HAL_SetRESET(bool high)
{
    if (g_reset.port != 0)
    {
        HAL_GPIO_WritePin(g_reset.port, g_reset.pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void ADS127_HAL_ToggleRESET(uint32_t pulse_ms)
{
    ADS127_HAL_SetRESET(false);
    ADS127_HAL_DelayMs(pulse_ms);
    ADS127_HAL_SetRESET(true);
}

void ADS127_HAL_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}
