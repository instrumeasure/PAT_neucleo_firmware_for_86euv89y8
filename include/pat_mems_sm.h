#ifndef PAT_MEMS_SM_H
#define PAT_MEMS_SM_H

#include "ad5664r.h"
#include "pat_mems_regs.h"
#include "stm32h7xx_hal.h"
#include <stdint.h>

typedef enum pat_mems_sm_state {
  PAT_MEMS_SM_OFF = 0,
  PAT_MEMS_SM_DAC_INIT = 1,
  PAT_MEMS_SM_FCLK_RUN = 2,
  PAT_MEMS_SM_ARMED = 3,
  PAT_MEMS_SM_EN_ON = 4
} pat_mems_sm_state_t;

typedef struct pat_mems_sm_ctx {
  pat_mems_reg_block_t *regs;
  ad5664r_dev_t *dac;
  TIM_HandleTypeDef *htim_x;
  uint32_t tim_x_ch;
  TIM_HandleTypeDef *htim_y;
  uint32_t tim_y_ch;
  GPIO_TypeDef *en_port;
  uint16_t en_pin;
  pat_mems_sm_state_t state;
  uint32_t entered_ms;
  uint32_t settle_ms;
} pat_mems_sm_ctx_t;

void pat_mems_sm_init(pat_mems_sm_ctx_t *ctx);
void pat_mems_sm_poll(pat_mems_sm_ctx_t *ctx, uint32_t now_ms);
pat_mems_sm_state_t pat_mems_sm_state(const pat_mems_sm_ctx_t *ctx);

#endif
