/** @file ultrasonic.h
 *  @brief Foreground measurement API and TIM1 interrupt entry for ultrasonic.
 */
#ifndef BRINCO_ULTRASONIC_H
#define BRINCO_ULTRASONIC_H

#include "main.h"

typedef enum {
  ULTRASONIC_NOT_READY = 0, /**< No measurement completed since startup. */
  ULTRASONIC_OK,            /**< A complete echo pulse produced a distance. */
  ULTRASONIC_TIMEOUT,       /**< No complete echo arrived before the deadline. */
  ULTRASONIC_INVALID_ECHO   /**< Stuck-high input, overcapture, or invalid pulse. */
} UltrasonicStatus;

typedef struct {
  UltrasonicStatus status; /**< Check before interpreting distance or pulse. */
  uint16_t distance_mm;    /**< Rounded, unsmoothed millimetres; zero on failure. */
  uint16_t pulse_us;       /**< Echo high time in microseconds; zero on failure. */
  uint32_t updated_ms;     /**< HAL tick when result/error was published. */
} UltrasonicReading;

/* Call after HAL_Init() and clock setup. TIM1 must receive the PCLK clock
 * without an APB prescaler. PB6 is TRIG; PA3 and PA1 share ECHO. */
void Ultrasonic_Init(void);
/* Run frequently in the foreground. Starts a measurement every 60 ms. */
void Ultrasonic_Process(void);
/* Foreground-only snapshot. Check status before using distance_mm. */
UltrasonicReading Ultrasonic_GetReading(void);
/** @brief Capture interrupt service; call only from TIM1_CC_IRQHandler. */
void Ultrasonic_TimerIRQ(void);

#endif
