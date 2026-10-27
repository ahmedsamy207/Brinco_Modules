/** @file tft_touch.h @brief Filtered touch, raw coordinates and RAM calibration. */
#ifndef BRINCO_TFT_TOUCH_H
#define BRINCO_TFT_TOUCH_H
#include <stdint.h>
typedef struct {
  uint16_t raw_x,raw_y,z,x,y; /* Pixel x/y=0xFFFF without calibrated contact. */
  uint8_t down,valid,calibrated;
  uint32_t updated_ms,sequence;
} TFTTouchReading;
void TFTTouch_Init(void);
/** @brief Sample every 20 ms between draw chunks; two stable contacts qualify down. */
void TFTTouch_Process(void);
TFTTouchReading TFTTouch_GetReading(void);
/** @brief Raw endpoints for native portrait left/right/top/bottom. Descending
 *  endpoints invert an axis. flags bit0 swaps raw axes before calibration.
 *  Each endpoint <=4095, each span >=128. Invalid settings change nothing.
 */
uint8_t TFTTouch_Calibrate(uint16_t x0,uint16_t x1,uint16_t y0,uint16_t y1,uint8_t flags);
#endif
