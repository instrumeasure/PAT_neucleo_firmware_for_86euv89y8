#include "app_state.h"

const char *app_state_to_string(app_state_t state)
{
    switch (state)
    {
    case APP_STATE_INIT:
        return "INIT";
    case APP_STATE_ADC_CFG:
        return "ADC_CFG";
    case APP_STATE_RUN:
        return "RUN";
    case APP_STATE_ERR_SPI:
        return "ERR_SPI";
    case APP_STATE_ERR_ADC:
        return "ERR_ADC";
    default:
        return "UNKNOWN";
    }
}
