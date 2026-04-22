#ifndef APP_STATE_H
#define APP_STATE_H

typedef enum
{
    APP_STATE_INIT = 0,
    APP_STATE_ADC_CFG,
    APP_STATE_RUN,
    APP_STATE_ERR_SPI,
    APP_STATE_ERR_ADC
} app_state_t;

const char *app_state_to_string(app_state_t state);

#endif
