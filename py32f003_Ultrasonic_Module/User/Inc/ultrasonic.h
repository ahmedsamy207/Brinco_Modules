#ifndef BRINCO_ULTRASONIC_H
#define BRINCO_ULTRASONIC_H

#include "main.h"

typedef enum {
  ULTRASONIC_NOT_READY = 0,
  ULTRASONIC_OK,
  ULTRASONIC_TIMEOUT,
  ULTRASONIC_INVALID_ECHO
} UltrasonicStatus;

typedef struct {
  UltrasonicStatus status;
  uint16_t distance_mm;
  uint16_t pulse_us;
  uint32_t updated_ms;
} UltrasonicReading;

/* Call after HAL_Init() and clock setup. TIM1 must receive the PCLK clock
 * without an APB prescaler. PB6 is TRIG; PA3 and PA1 share ECHO. */
void Ultrasonic_Init(void);
/* Run frequently in the foreground. Starts a measurement every 60 ms. */
void Ultrasonic_Process(void);
/* Foreground-only snapshot. Check status before using distance_mm. */
UltrasonicReading Ultrasonic_GetReading(void);
void Ultrasonic_TimerIRQ(void);

#endif
