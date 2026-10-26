/** @file color_sensor.h @brief Foreground RGBW acquisition and configuration. */
#ifndef BRINCO_COLOR_SENSOR_H
#define BRINCO_COLOR_SENSOR_H
#include <stdint.h>
typedef struct {
  uint16_t channels[4]; /* R,G,B,W raw 16-bit counts; not display RGB/sRGB. */
  uint16_t integration_ms; /* Requested exposure; invalidates old sample on change. */
  uint8_t led, valid, error, saturation; /* Saturation mask bits 0..3 = R,G,B,W. */
  uint32_t updated_ms, sequence; /* Last complete read and wrapping sample counter. */
} ColorReading;
void ColorSensor_Init(void);
/** @brief One bounded I2C transaction per foreground service, no integration delay. */
void ColorSensor_Process(void);
/** @brief Snapshot with freshness check; foreground-only like all APIs here. */
ColorReading ColorSensor_GetReading(void);
/** @brief Accept only 40/80/160/320/640/1280 ms; 0 means rejected/no change. */
uint8_t ColorSensor_SetIntegration(uint16_t milliseconds);
/** @brief Accept only 0/1. Changed illumination invalidates and restarts acquisition. */
uint8_t ColorSensor_SetLED(uint8_t on);
#endif
