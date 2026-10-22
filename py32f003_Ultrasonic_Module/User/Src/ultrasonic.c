#include "ultrasonic.h"

#define MEASUREMENT_INTERVAL_MS 60U
#define ECHO_TIMEOUT_US         30000U
#define ECHO_TIMEOUT_MS         30U
#define TRIGGER_US              10U
#define CAPTURE_FLAGS (TIM_FLAG_CC1 | TIM_FLAG_CC4 | TIM_FLAG_CC1OF | TIM_FLAG_CC4OF)

typedef enum { CAPTURE_IDLE, CAPTURE_RISING, CAPTURE_FALLING,
               CAPTURE_DONE, CAPTURE_INVALID } CaptureState;

static TIM_HandleTypeDef timer;
static volatile CaptureState capture_state;
static volatile uint16_t rising_us;
static volatile uint16_t pulse_us;
static uint32_t started_ms;
static uint32_t last_trigger_ms;
static UltrasonicReading reading;

void Ultrasonic_Init(void)
{
  TIM_IC_InitTypeDef input = {0};
  GPIO_InitTypeDef gpio = {0};
  uint32_t timer_hz = HAL_RCC_GetPCLK1Freq();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE();

  /* Set the output latch before enabling the pin to avoid a startup pulse. */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_6;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_1 | GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Alternate = GPIO_AF13_TIM1;
  HAL_GPIO_Init(GPIOA, &gpio);

  if (timer_hz < 1000000U || timer_hz % 1000000U != 0U) {
    APP_ErrorHandler();
  }
  timer.Instance = TIM1;
  timer.Init.Prescaler = timer_hz / 1000000U - 1U;
  timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  timer.Init.Period = 65535U;
  timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&timer) != HAL_OK) APP_ErrorHandler();

  input.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  input.ICSelection = TIM_ICSELECTION_DIRECTTI;
  input.ICPrescaler = TIM_ICPSC_DIV1;
  input.ICFilter = 3U;
  if (HAL_TIM_IC_ConfigChannel(&timer, &input, TIM_CHANNEL_1) != HAL_OK) {
    APP_ErrorHandler();
  }
  input.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  if (HAL_TIM_IC_ConfigChannel(&timer, &input, TIM_CHANNEL_4) != HAL_OK) {
    APP_ErrorHandler();
  }
  capture_state = CAPTURE_IDLE;
  if (HAL_TIM_IC_Start(&timer, TIM_CHANNEL_1) != HAL_OK ||
      HAL_TIM_IC_Start(&timer, TIM_CHANNEL_4) != HAL_OK) APP_ErrorHandler();
  HAL_NVIC_SetPriority(TIM1_CC_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
  /* Allow the sensor to settle before the first trigger. */
  last_trigger_ms = HAL_GetTick();
}

void Ultrasonic_TimerIRQ(void)
{
  uint32_t flags = TIM1->SR;
  uint16_t rise = (uint16_t)TIM1->CCR1;
  uint16_t fall = (uint16_t)TIM1->CCR4;
  __HAL_TIM_CLEAR_FLAG(&timer, CAPTURE_FLAGS);

  if (capture_state != CAPTURE_RISING && capture_state != CAPTURE_FALLING) return;
  if ((flags & (TIM_FLAG_CC1OF | TIM_FLAG_CC4OF)) != 0U) {
    capture_state = CAPTURE_INVALID;
  } else {
    if ((flags & TIM_FLAG_CC1) != 0U) {
      if (capture_state != CAPTURE_RISING) {
        capture_state = CAPTURE_INVALID;
      } else {
        rising_us = rise;
        capture_state = CAPTURE_FALLING;
      }
    }
    if ((flags & TIM_FLAG_CC4) != 0U && capture_state == CAPTURE_FALLING) {
      /* Unsigned 16-bit subtraction handles the 65.536 ms timer wrap. */
      pulse_us = (uint16_t)(fall - rising_us);
      capture_state = pulse_us != 0U && pulse_us < ECHO_TIMEOUT_US
                    ? CAPTURE_DONE : CAPTURE_INVALID;
    }
  }
  if (capture_state == CAPTURE_DONE || capture_state == CAPTURE_INVALID) {
    __HAL_TIM_DISABLE_IT(&timer, TIM_IT_CC1 | TIM_IT_CC4);
  }
}

void Ultrasonic_Process(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t mask = __get_PRIMASK();
  CaptureState state;
  uint16_t duration;

  __disable_irq();
  state = capture_state;
  duration = pulse_us;
  if (state == CAPTURE_DONE || state == CAPTURE_INVALID ||
      (state != CAPTURE_IDLE && (uint32_t)(now - started_ms) >= ECHO_TIMEOUT_MS)) {
    __HAL_TIM_DISABLE_IT(&timer, TIM_IT_CC1 | TIM_IT_CC4);
    capture_state = CAPTURE_IDLE;
    reading.status = state == CAPTURE_DONE ? ULTRASONIC_OK :
                     state == CAPTURE_INVALID ? ULTRASONIC_INVALID_ECHO : ULTRASONIC_TIMEOUT;
    reading.pulse_us = state == CAPTURE_DONE ? duration : 0U;
    /* Speed of sound 343 m/s at about 20 C; round to nearest millimetre. */
    reading.distance_mm = state == CAPTURE_DONE
                       ? (uint16_t)(((uint32_t)duration * 343U + 1000U) / 2000U) : 0U;
    reading.updated_ms = now;
  }
  state = capture_state;
  __set_PRIMASK(mask);

  if (state == CAPTURE_IDLE && (uint32_t)(now - last_trigger_ms) >= MEASUREMENT_INTERVAL_MS) {
    uint16_t trigger_start;
    last_trigger_ms = now;
    /* A stuck-high ECHO is a failed measurement, not a new rising edge. */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET) {
      reading.status = ULTRASONIC_INVALID_ECHO;
      reading.distance_mm = 0U;
      reading.pulse_us = 0U;
      reading.updated_ms = now;
      return;
    }
    mask = __get_PRIMASK();
    __disable_irq();
    __HAL_TIM_CLEAR_FLAG(&timer, CAPTURE_FLAGS);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
    started_ms = now;
    capture_state = CAPTURE_RISING;
    __HAL_TIM_ENABLE_IT(&timer, TIM_IT_CC1 | TIM_IT_CC4);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    trigger_start = (uint16_t)__HAL_TIM_GET_COUNTER(&timer);
    while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(&timer) - trigger_start) < TRIGGER_US) {}
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    __set_PRIMASK(mask);
  }
}

UltrasonicReading Ultrasonic_GetReading(void)
{
  return reading;
}
