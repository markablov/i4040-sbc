#include "main.h"
#include "tim.h"
#include "init.h"

/*
 * For MCS-4 processor we need to provide two-phase clock with period tCY = 1350ns (targetFreq = 740kHz) and width tPW = 380ns (active LOW)
 * Offset between rising edge of phi1 and rising edge of phi2 should be tD1 + tPW = 400ns + 380ns = 780ns
 *
 * To achieve that master-slave timers are used with same period (frequency): TIM_Period = (timerFreq / targetFreq) - 1 = 168Mhz / 0.74Mhz - 1 = 226
 * Pulse width should be (TIM_Period + 1) * (targetPeriod / timerPeriod) - 1 = 227 * ((1350ns - 380ns) / 1350ns) - 1 = 162
 * Phase offset should be (TIM_Period + 1) * (targetOffset / timerPeriod) - 1 = 227 * (780ns / 1350ns) - 1 = 130
 */
static void initMCS4Clocks() {
  HAL_TIM_Base_Start(&htim8);
  HAL_TIM_PWM_Start_IT(&htim8, TIM_CHANNEL_4);
  HAL_TIM_Base_Start(&htim1);
  HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
  HAL_TIM_OC_Start(&htim1, TIM_CHANNEL_2);
}

void init(void) {
  OUT_TEST0_GPIO_Port->ODR &= ~OUT_TEST0_Pin;

  initMCS4Clocks();
}