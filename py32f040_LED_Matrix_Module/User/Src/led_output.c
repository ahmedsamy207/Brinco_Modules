/** @file led_output.c
 *  @brief Generate LED bits entirely in TIM15 hardware with update-request DMA.
 *  The timer/compare preload pipeline is surrounded by 300 us zero-duty runs.
 *  DMA completion therefore occurs while DIN is already low, after the final
 *  reset interval; stopping PWM cannot truncate a data bit. SPI IRQs stay on.
 */
#include "main.h"
#include "led_output.h"
#include "led_waveform.h"

static TIM_HandleTypeDef timer;
static DMA_HandleTypeDef dma;
static uint16_t waveform[MATRIX_WAVEFORM_WORDS];
static volatile uint8_t busy, failed;
static uint32_t started_ms;

/** @brief Actively hold DIN low when the timer output is disabled. */
static void HoldLow(void)
{
  GPIO_InitTypeDef gpio = {0};
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_15;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);
}

/** @brief Stop requests before disabling PWM; called from IRQ or watchdog. */
static void StopOutput(uint8_t error)
{
  __HAL_TIM_DISABLE_DMA(&timer, TIM_DMA_UPDATE);
  (void)HAL_TIM_PWM_Stop(&timer, TIM_CHANNEL_2);
  HoldLow();
  if (error) failed = 1U;
  busy = 0U;
}
static void Complete(DMA_HandleTypeDef *handle) { (void)handle; StopOutput(0U); }
static void DMAError(DMA_HandleTypeDef *handle) { (void)handle; StopOutput(1U); }

void LEDOutput_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  TIM_OC_InitTypeDef pwm = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_TIM15_CLK_ENABLE();
  __HAL_RCC_DMA_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  /* Keep DIN low before enabling its timer alternate function. */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_15;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);
  if (HAL_RCC_GetPCLK1Freq() != MATRIX_TIMER_HZ) APP_ErrorHandler();
  timer.Instance = TIM15;
  timer.Init.Prescaler = 0U;
  timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  timer.Init.Period = MATRIX_PERIOD_TICKS - 1U;
  timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&timer) != HAL_OK) APP_ErrorHandler();
  pwm.OCMode = TIM_OCMODE_PWM1;
  pwm.Pulse = 0U;
  pwm.OCPolarity = TIM_OCPOLARITY_HIGH;
  pwm.OCFastMode = TIM_OCFAST_DISABLE;
  pwm.OCIdleState = TIM_OCIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&timer, &pwm, TIM_CHANNEL_2) != HAL_OK) APP_ErrorHandler();
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Alternate = GPIO_AF1_TIM15;
  HAL_GPIO_Init(GPIOB, &gpio);

  dma.Instance = DMA1_Channel1;
  dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
  dma.Init.PeriphInc = DMA_PINC_DISABLE;
  dma.Init.MemInc = DMA_MINC_ENABLE;
  dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma.Init.Mode = DMA_NORMAL;
  dma.Init.Priority = DMA_PRIORITY_VERY_HIGH;
  if (HAL_DMA_Init(&dma) != HAL_OK) APP_ErrorHandler();
  HAL_DMA_ChannelMap(&dma, DMA_CHANNEL_MAP_TIM15_UP);
  dma.XferCpltCallback = Complete;
  dma.XferErrorCallback = DMAError;
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

uint8_t LEDOutput_Start(const uint8_t *rgb, uint8_t brightness)
{
  GPIO_InitTypeDef gpio = {0};
  if (busy || failed) return 0U;
  LEDWaveform_Encode(rgb, brightness, waveform);
  __HAL_TIM_DISABLE(&timer);
  __HAL_TIM_SET_COUNTER(&timer, 0U);
  __HAL_TIM_SET_COMPARE(&timer, TIM_CHANNEL_2, 0U);
  /* Load zero into the active compare before DMA starts filling its preload. */
  timer.Instance->EGR = TIM_EGR_UG;
  __HAL_TIM_CLEAR_FLAG(&timer, TIM_FLAG_UPDATE);
  if (HAL_DMA_Start_IT(&dma, (uint32_t)waveform, (uint32_t)&TIM15->CCR2,
                       MATRIX_WAVEFORM_WORDS) != HAL_OK) {
    failed = 1U;
    return 0U;
  }
  started_ms = HAL_GetTick();
  busy = 1U;
  gpio.Pin = GPIO_PIN_15;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF1_TIM15;
  HAL_GPIO_Init(GPIOB, &gpio);
  __HAL_TIM_ENABLE_DMA(&timer, TIM_DMA_UPDATE);
  if (HAL_TIM_PWM_Start(&timer, TIM_CHANNEL_2) != HAL_OK) {
    (void)HAL_DMA_Abort(&dma);
    StopOutput(1U);
    return 0U;
  }
  return 1U;
}

uint8_t LEDOutput_IsBusy(void) { return busy; }
uint8_t LEDOutput_HasError(void) { return failed; }
void LEDOutput_DMAIRQ(void) { HAL_DMA_IRQHandler(&dma); }
void LEDOutput_Process(void)
{
  if (busy && (uint32_t)(HAL_GetTick() - started_ms) > MATRIX_DMA_TIMEOUT_MS) {
    /* No shared buffer is reusable until the timed-out DMA has been stopped. */
    __HAL_TIM_DISABLE_DMA(&timer, TIM_DMA_UPDATE);
    (void)HAL_DMA_Abort(&dma);
    StopOutput(1U);
  }
}
