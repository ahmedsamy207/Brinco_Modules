/** @file touch_sensor.h
 *  @brief Foreground-only touch acquisition API. No IRQ accesses driver state.
 */
#ifndef BRINCO_TOUCH_SENSOR_H
#define BRINCO_TOUCH_SENSOR_H
#include <stdint.h>
#include "touch_sensor_config.h"

typedef struct {
  uint8_t raw_level;    /**< Observed PA2 voltage: 1 = high, independent of polarity. */
  uint8_t touched;      /**< Qualified logical state; interpret only when valid != 0. */
  uint8_t valid;        /**< Zero during startup, initial qualification or stale service. */
  uint32_t updated_ms;  /**< HAL tick of last input observation, not last state change. */
} TouchReading;

/** @brief Configure PA2 as input; begin a fresh startup guard from the HAL tick. */
void TouchSensor_Init(void);
/** @brief Service frequently from main(); qualify both touch and release edges. */
void TouchSensor_Process(void);
/** @brief Return a cached snapshot, invalidated if foreground sampling is stale. */
TouchReading TouchSensor_GetReading(void);
#endif
