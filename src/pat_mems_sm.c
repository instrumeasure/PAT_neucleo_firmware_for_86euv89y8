#include "pat_mems_sm.h"

static void mems_en_set(const pat_mems_sm_ctx_t *ctx, GPIO_PinState s)
{
  if ((ctx == NULL) || (ctx->en_port == NULL)) {
    return;
  }
  HAL_GPIO_WritePin(ctx->en_port, ctx->en_pin, s);
}

static void mems_fclk_set(const pat_mems_sm_ctx_t *ctx, int on)
{
  if (ctx == NULL) {
    return;
  }
  if (on) {
    (void)HAL_TIM_PWM_Start(ctx->htim_x, ctx->tim_x_ch);
    (void)HAL_TIM_PWM_Start(ctx->htim_y, ctx->tim_y_ch);
  } else {
    (void)HAL_TIM_PWM_Stop(ctx->htim_x, ctx->tim_x_ch);
    (void)HAL_TIM_PWM_Stop(ctx->htim_y, ctx->tim_y_ch);
  }
}

void pat_mems_sm_init(pat_mems_sm_ctx_t *ctx)
{
  if ((ctx == NULL) || (ctx->regs == NULL)) {
    return;
  }
  ctx->state = PAT_MEMS_SM_OFF;
  ctx->entered_ms = HAL_GetTick();
  ctx->settle_ms = 2u;
  mems_en_set(ctx, GPIO_PIN_RESET);
  mems_fclk_set(ctx, 0);
}

static void sm_enter(pat_mems_sm_ctx_t *ctx, pat_mems_sm_state_t s, uint32_t now)
{
  ctx->state = s;
  ctx->entered_ms = now;
}

void pat_mems_sm_poll(pat_mems_sm_ctx_t *ctx, uint32_t now_ms)
{
  uint32_t ctrl;
  if ((ctx == NULL) || (ctx->regs == NULL)) {
    return;
  }
  ctrl = ctx->regs->ctrl;

  switch (ctx->state) {
  case PAT_MEMS_SM_OFF:
    mems_en_set(ctx, GPIO_PIN_RESET);
    if (ad5664r_init_sequence(ctx->dac, 0x8000u) == HAL_OK) {
      sm_enter(ctx, PAT_MEMS_SM_DAC_INIT, now_ms);
    }
    break;
  case PAT_MEMS_SM_DAC_INIT:
    mems_fclk_set(ctx, 1);
    sm_enter(ctx, PAT_MEMS_SM_FCLK_RUN, now_ms);
    break;
  case PAT_MEMS_SM_FCLK_RUN:
    mems_en_set(ctx, GPIO_PIN_RESET);
    if ((ctrl & PAT_MEMS_CTRL_ARM) != 0u && (now_ms - ctx->entered_ms) >= ctx->settle_ms) {
      sm_enter(ctx, PAT_MEMS_SM_ARMED, now_ms);
    }
    break;
  case PAT_MEMS_SM_ARMED:
    mems_en_set(ctx, GPIO_PIN_RESET);
    if ((ctrl & PAT_MEMS_CTRL_ARM) == 0u) {
      sm_enter(ctx, PAT_MEMS_SM_FCLK_RUN, now_ms);
    } else if ((ctrl & PAT_MEMS_CTRL_EN_REQ) != 0u) {
      mems_en_set(ctx, GPIO_PIN_SET);
      sm_enter(ctx, PAT_MEMS_SM_EN_ON, now_ms);
    }
    break;
  case PAT_MEMS_SM_EN_ON:
    if (((ctrl & PAT_MEMS_CTRL_ARM) == 0u) || ((ctrl & PAT_MEMS_CTRL_EN_REQ) == 0u)) {
      mems_en_set(ctx, GPIO_PIN_RESET);
      sm_enter(ctx, PAT_MEMS_SM_ARMED, now_ms);
    }
    break;
  default:
    sm_enter(ctx, PAT_MEMS_SM_OFF, now_ms);
    break;
  }
}

pat_mems_sm_state_t pat_mems_sm_state(const pat_mems_sm_ctx_t *ctx)
{
  if (ctx == NULL) {
    return PAT_MEMS_SM_OFF;
  }
  return ctx->state;
}
