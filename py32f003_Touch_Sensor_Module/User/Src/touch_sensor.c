/**
 * @file touch_sensor.c
 * @brief Sample the capacitive controller's digital OUT and qualify its state.
 *
 * OUT connects to PA2/package pin 1. The sensing IC, electrode network and
 * indicator LED operate in hardware; the MCU never drives OUT or the electrode.
 * Sampling/debounce run only in the foreground. A held input stays held; this
 * layer does not toggle or latch presses. The 223 IC itself limits a direct
 * output activation to 8 s; software does not conceal its automatic release.
 * All tick arithmetic tolerates wrap.
 */
#include "main.h"
#include "touch_sensor.h"

static TouchReading reading;
static uint32_t boot_ms;
static uint32_t last_sample_ms;
static uint32_t candidate_since_ms;
static uint8_t candidate;
static uint8_t tracking;
static uint8_t startup_complete;

/** @brief Initialize the push-pull OUT input without internal pull resistors.
 *  @note Do not infer the startup state as released: wait for guard + debounce.
 */
void TouchSensor_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_2;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  reading = (TouchReading){0};
  reading.raw_level = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET;
  boot_ms = HAL_GetTick();
  reading.updated_ms = boot_ms;
  last_sample_ms = boot_ms;
  candidate_since_ms = boot_ms;
  candidate = 0U;
  tracking = 0U;
  startup_complete = 0U;
}

/** @brief Observe PA2 and accept a logical state after a stable debounce window.
 *  @note Previously qualified state remains valid while a new edge is pending.
 *        A long sampling gap invalidates it and restarts qualification, because
 *        the input's unobserved history cannot establish stability.
 */
void TouchSensor_Process(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t elapsed = now - last_sample_ms;
  uint8_t logical;
  if (elapsed < TOUCH_SAMPLE_MS) return;
  last_sample_ms = now;
  reading.raw_level = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET;
  reading.updated_ms = now;
  logical = TOUCH_ACTIVE_HIGH ? reading.raw_level : !reading.raw_level;

  if (!startup_complete) {
    if ((uint32_t)(now - boot_ms) < TOUCH_STARTUP_MS) {
      reading.valid = 0U;
      tracking = 0U;
      return;
    }
    /* Latch completion so a later full HAL tick wrap cannot re-enter startup. */
    startup_complete = 1U;
  }
  if (elapsed > TOUCH_STALE_MS) {
    reading.valid = 0U;
    tracking = 0U;
  }
  if (!tracking || logical != candidate) {
    candidate = logical;
    candidate_since_ms = now;
    tracking = 1U;
  } else if ((uint32_t)(now - candidate_since_ms) >= TOUCH_DEBOUNCE_MS) {
    reading.touched = candidate;
    reading.valid = 1U;
  }
}

/** @brief Return the latest observation and qualified state to the UART service.
 *  @note A stale snapshot is invalid, even if the last accepted state was held.
 *        raw_level is diagnostic only and never substitutes for valid state.
 */
TouchReading TouchSensor_GetReading(void)
{
  TouchReading snapshot = reading;
  if ((uint32_t)(HAL_GetTick() - snapshot.updated_ms) > TOUCH_STALE_MS) {
    snapshot.valid = 0U;
  }
  return snapshot;
}
